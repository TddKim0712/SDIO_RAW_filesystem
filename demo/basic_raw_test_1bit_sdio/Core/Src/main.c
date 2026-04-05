/*
 * RAW-ONLY SD bring-up example for STM32F4 + SDIO + HAL.
 *
 * Assumptions:
 *   - Cube generated gpio.c, dma.c, usart.c, sdio.c are present.
 *   - sdio.c only fills hsd.Init fields. It must NOT call HAL_SD_Init()
 *     and must NOT call HAL_SD_ConfigWideBusOperation(...4B).
 *   - This example intentionally destroys any FAT/partition layout.
 *
 * Result after boot:
 *   - LBA 0 : raw superblock with magic "RAWSDIO1"
 *   - LBA 1 : pattern block #1
 *   - LBA 2 : pattern block #2
 *   - LBA 3 : pattern block #3
 *
 * Host-side verification:
 *   Use \\.\PhysicalDriveN, not drive letters.
 */

#include "main.h"
#include "gpio.h"
#include "dma.h"
#include "usart.h"
#include "sdio.h"
#include "raw_diskio.h"

#include <stdio.h>
#include <string.h>

#define RAW_LAYOUT_VERSION      1U
#define RAW_DATA_START_LBA      1U
#define RAW_PATTERN_BLOCK_COUNT 3U
#define RAW_TAIL_MAGIC          0xA55A5AA5UL

extern SD_HandleTypeDef hsd;
extern UART_HandleTypeDef huart2;

#pragma pack(push, 1)
typedef struct
{
    char     magic[8];               /* "RAWSDIO1" */
    uint32_t version;                /* 1 */
    uint32_t block_size;             /* 512 */
    uint32_t card_block_count;       /* from HAL_SD_GetCardInfo */
    uint32_t data_start_lba;         /* usually 1 */
    uint32_t boot_count;             /* increments every boot if desired */
    uint32_t pattern_start_lba;      /* 1 */
    uint32_t pattern_block_count;    /* 3 */
    uint8_t  reserved[472];
    uint32_t tail_magic;             /* 0xA55A5AA5 */
} raw_superblock_t;
#pragma pack(pop)

typedef char raw_superblock_size_must_be_512[(sizeof(raw_superblock_t) == 512U) ? 1 : -1];

static HAL_SD_CardInfoTypeDef g_card_info;
static uint32_t g_tx_block[RAW_SD_BLOCK_SIZE / sizeof(uint32_t)];
static uint32_t g_rx_block[RAW_SD_BLOCK_SIZE / sizeof(uint32_t)];
static uint32_t g_boot_count = 1U;

void SystemClock_Config(void);

static void fatal(const char *tag)
{
    printf("[FATAL] %s, hal_sd_err=0x%08lX\r\n",
           tag,
           (unsigned long)raw_sd_get_last_error());
    Error_Handler();
}

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1U, HAL_MAX_DELAY);
    return ch;
}

static void build_superblock(raw_superblock_t *sb)
{
    memset(sb, 0, sizeof(*sb));
    memcpy(sb->magic, "RAWSDIO1", 8U);
    sb->version = RAW_LAYOUT_VERSION;
    sb->block_size = RAW_SD_BLOCK_SIZE;
    sb->card_block_count = g_card_info.LogBlockNbr;
    sb->data_start_lba = RAW_DATA_START_LBA;
    sb->boot_count = g_boot_count;
    sb->pattern_start_lba = RAW_DATA_START_LBA;
    sb->pattern_block_count = RAW_PATTERN_BLOCK_COUNT;
    sb->tail_magic = RAW_TAIL_MAGIC;
}

static void build_pattern_block(uint32_t lba, uint8_t *dst)
{
    uint32_t i;

    memset(dst, 0x00, RAW_SD_BLOCK_SIZE);

    memcpy(&dst[0],  "RAWBLK", 6U);
    dst[6] = (uint8_t)('0' + (lba % 10U));
    dst[7] = '!';

    memcpy(&dst[8],  &lba, sizeof(lba));
    memcpy(&dst[12], &g_boot_count, sizeof(g_boot_count));

    for (i = 16U; i < RAW_SD_BLOCK_SIZE; i++)
    {
        dst[i] = (uint8_t)((i ^ lba) & 0xFFU);
    }
}

static void print_first_32(const uint8_t *buf)
{
    uint32_t i;

    printf("[HEX] ");
    for (i = 0U; i < 32U; i++)
    {
        printf("%02X ", buf[i]);
    }
    printf("\r\n");
}

static void verify_superblock(const raw_superblock_t *sb)
{
    if (memcmp(sb->magic, "RAWSDIO1", 8U) != 0)
    {
        fatal("verify superblock magic");
    }
    if (sb->version != RAW_LAYOUT_VERSION)
    {
        fatal("verify superblock version");
    }
    if (sb->block_size != RAW_SD_BLOCK_SIZE)
    {
        fatal("verify superblock block_size");
    }
    if (sb->tail_magic != RAW_TAIL_MAGIC)
    {
        fatal("verify superblock tail");
    }
}

static void verify_pattern_block(uint32_t lba, const uint8_t *src)
{
    uint32_t i;
    uint8_t expected;

    if (memcmp(&src[0], "RAWBLK", 6U) != 0)
    {
        fatal("verify pattern magic");
    }

    for (i = 16U; i < RAW_SD_BLOCK_SIZE; i++)
    {
        expected = (uint8_t)((i ^ lba) & 0xFFU);
        if (src[i] != expected)
        {
            printf("[VERIFY] lba=%lu mismatch at byte %lu got=0x%02X exp=0x%02X\r\n",
                   (unsigned long)lba,
                   (unsigned long)i,
                   src[i],
                   expected);
            fatal("verify pattern data");
        }
    }
}

int main(void)
{
    uint32_t lba;
    raw_superblock_t *sb_tx = (raw_superblock_t *)g_tx_block;
    raw_superblock_t *sb_rx = (raw_superblock_t *)g_rx_block;

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART2_UART_Init();
    MX_SDIO_SD_Init();

    printf("\r\n[BOOT] RAW-ONLY SD mode\r\n");
    printf("[WARN] This layout destroys FAT/partition information.\r\n");

    if (raw_sd_init() != RAW_SD_OK)
    {
        fatal("raw_sd_init");
    }

    raw_sd_get_card_info(&g_card_info);
    printf("[CARD] block_count=%lu block_size=%lu type=%lu\r\n",
           (unsigned long)g_card_info.LogBlockNbr,
           (unsigned long)g_card_info.LogBlockSize,
           (unsigned long)g_card_info.CardType);

    build_superblock(sb_tx);
    if (raw_sd_write_blocks(0U, g_tx_block, 1U) != RAW_SD_OK)
    {
        fatal("write superblock");
    }
    memset(g_rx_block, 0, sizeof(g_rx_block));
    if (raw_sd_read_blocks(0U, g_rx_block, 1U) != RAW_SD_OK)
    {
        fatal("read superblock");
    }
    verify_superblock(sb_rx);
    printf("[OK] LBA0 superblock written\r\n");
    print_first_32((const uint8_t *)g_rx_block);

    for (lba = RAW_DATA_START_LBA; lba < (RAW_DATA_START_LBA + RAW_PATTERN_BLOCK_COUNT); lba++)
    {
        build_pattern_block(lba, (uint8_t *)g_tx_block);
        if (raw_sd_write_blocks(lba, g_tx_block, 1U) != RAW_SD_OK)
        {
            fatal("write pattern block");
        }

        memset(g_rx_block, 0, sizeof(g_rx_block));
        if (raw_sd_read_blocks(lba, g_rx_block, 1U) != RAW_SD_OK)
        {
            fatal("read pattern block");
        }

        verify_pattern_block(lba, (const uint8_t *)g_rx_block);
        printf("[OK] LBA%lu pattern written\r\n", (unsigned long)lba);
        print_first_32((const uint8_t *)g_rx_block);
    }

    printf("[DONE] Raw layout now occupies LBA0..3\r\n");
    printf("[HOST] Use \\\\.\\PhysicalDriveN only. Drive letters are no longer valid.\r\n");

    while (1)
    {
        HAL_Delay(1000U);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
