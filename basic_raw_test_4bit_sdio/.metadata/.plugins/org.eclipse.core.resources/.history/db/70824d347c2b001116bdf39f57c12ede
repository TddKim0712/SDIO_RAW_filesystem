#include <app_sd_test_config.h>
#include "main.h"
#include "gpio.h"
#include "dma.h"
#include "usart.h"
#include "sdio.h"

#include "raw_diskio.h"
#include "raw_log_writer.h"

#include <stdio.h>

/* Private variables ---------------------------------------------------------*/
extern UART_HandleTypeDef huart2;

static raw_log_writer_t g_raw_writer;
static uint8_t g_dummy_payload[RAW_LOG_DATA_PAYLOAD_BYTES_V3];

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

static void App_Init(void);
static void App_PrintBanner(void);
static void App_PrintCardInfo(void);
static void App_PrintStartState(void);
static void App_RunRawWriterLoop(void);
static void App_PreparePayload(uint8_t *payload, uint32_t payload_bytes);
static void App_HandleWriteStep(const raw_log_writer_step_info_t *step);
static void App_Fatal(const char *tag);

/* Private user code ---------------------------------------------------------*/
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1U, HAL_MAX_DELAY);
    return ch;
}

static void App_Init(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART2_UART_Init();
    MX_SDIO_SD_Init();
}

static void App_PrintBanner(void)
{
#if (APP_ENABLE_UART_LOG != 0U)
    printf("\r\n[BOOT] RAW V3 continuous writer\r\n");
    printf("[CFG] test_mode=%s clk_div=%lu payload=%lu packets_per_block=%lu\r\n",
           APP_SD_SELECTED_BUS_WIDTH_NAME,
           (unsigned long)APP_SD_SELECTED_CLOCK_DIV,
           (unsigned long)RAW_LOG_DATA_PAYLOAD_BYTES_V3,
           (unsigned long)RAW_LOG_PACKETS_PER_DATA_BLOCK_V3);
    printf("[CFG] superblock_interval=%lu stall_threshold_ms=%lu\r\n",
           (unsigned long)g_raw_writer.cfg.superblock_write_interval,
           (unsigned long)g_raw_writer.cfg.stall_threshold_ms);
#endif
}

static void App_PrintCardInfo(void)
{
#if (APP_ENABLE_UART_LOG != 0U)
    printf("[CARD] block_count=%lu block_size=%lu type=%lu data_start=%lu ring_start=%lu ring_count=%lu\r\n",
           (unsigned long)g_raw_writer.card_info.LogBlockNbr,
           (unsigned long)g_raw_writer.card_info.LogBlockSize,
           (unsigned long)g_raw_writer.card_info.CardType,
           (unsigned long)g_raw_writer.cfg.data_start_lba,
           (unsigned long)g_raw_writer.cfg.superblock_ring_start_lba,
           (unsigned long)g_raw_writer.cfg.superblock_ring_count);
#endif
}

static void App_PrintStartState(void)
{
#if (APP_ENABLE_UART_LOG != 0U)
    printf("[START] boot_count=%lu next_data_lba=%lu next_seq=%lu last_lba=%lu\r\n",
           (unsigned long)g_raw_writer.state.boot_count,
           (unsigned long)g_raw_writer.state.next_data_lba,
           (unsigned long)g_raw_writer.state.write_seq,
           (unsigned long)g_raw_writer.state.last_written_lba);
#endif
}

static void App_PreparePayload(uint8_t *payload, uint32_t payload_bytes)
{
    if (payload == NULL)
    {
        return;
    }

#if (APP_ENABLE_TEST_PATTERN_PAYLOAD != 0U)
    raw_log_writer_fill_test_payload(payload,
                                     payload_bytes,
                                     g_raw_writer.state.write_seq,
                                     g_raw_writer.state.next_data_lba);
#else
    (void)payload_bytes;
#endif
}

static void App_HandleWriteStep(const raw_log_writer_step_info_t *step)
{
    if (step == NULL)
    {
        return;
    }

#if (APP_ENABLE_UART_LOG != 0U)
    if (step->stall != 0U)
    {
        printf("[STALL] seq=%lu data_lba=%lu sb=%lu sb_lba=%lu total_ms=%lu data_ms=%lu sb_ms=%lu stall_count=%lu\r\n",
               (unsigned long)step->seq,
               (unsigned long)step->data_lba,
               (unsigned long)step->superblock_written,
               (unsigned long)step->superblock_lba,
               (unsigned long)step->total_ms,
               (unsigned long)step->data_ms,
               (unsigned long)step->superblock_ms,
               (unsigned long)step->stall_count);
    }

    if ((g_raw_writer.state.write_seq % g_raw_writer.log_every_n_blocks) == 0U)
    {
        printf("[LOG] seq=%lu data_lba=%lu next_lba=%lu sb=%lu sb_lba=%lu total_ms=%lu max_ms=%lu stalls=%lu\r\n",
               (unsigned long)step->seq,
               (unsigned long)step->data_lba,
               (unsigned long)step->next_data_lba,
               (unsigned long)step->superblock_written,
               (unsigned long)step->superblock_lba,
               (unsigned long)step->total_ms,
               (unsigned long)g_raw_writer.state.max_total_write_ms,
               (unsigned long)g_raw_writer.state.stall_count);
    }
#else
    (void)step;
#endif
}

static void App_RunRawWriterLoop(void)
{
    raw_log_writer_result_t writer_result;
    raw_log_writer_step_info_t step;

    for (;;)
    {
        App_PreparePayload(g_dummy_payload, sizeof(g_dummy_payload));

        writer_result = raw_log_writer_write_payload(&g_raw_writer,
                                                     g_dummy_payload,
                                                     sizeof(g_dummy_payload),
                                                     &step);
        if (writer_result != RAW_LOG_WRITER_OK)
        {
            App_Fatal("raw_log_writer_write_payload");
        }

        App_HandleWriteStep(&step);
    }
}

static void App_Fatal(const char *tag)
{
#if (APP_ENABLE_UART_LOG != 0U)
    printf("[FATAL] %s hal=0x%08lX sta=0x%08lX card=%lu\r\n",
           tag,
           (unsigned long)raw_sd_get_last_error(),
           (unsigned long)hsd.Instance->STA,
           (unsigned long)HAL_SD_GetCardState(&hsd));
#endif

    Error_Handler();
}

int main(void)
{
    raw_log_writer_result_t writer_status;

    App_Init();

    raw_log_writer_init_defaults(&g_raw_writer);
    g_raw_writer.log_every_n_blocks = APP_LOG_EVERY_N_BLOCKS;

    App_PrintBanner();

    /*
     * Card identification always starts at <= 400 kHz inside raw_sd_init().
     * The selected transfer bus width and transfer clock are then applied.
     * Edit only app_sd_test_config.h while comparing 1-bit and 4-bit runs.
     */
    if (raw_sd_init(APP_SD_SELECTED_BUS_WIDTH, APP_SD_SELECTED_CLOCK_DIV) != RAW_SD_OK)
    {
        App_Fatal("raw_sd_init");
    }

    writer_status = raw_log_writer_begin(&g_raw_writer);
    if (writer_status != RAW_LOG_WRITER_OK)
    {
        App_Fatal("raw_log_writer_begin");
    }

    App_PrintCardInfo();
    App_PrintStartState();

    if (g_raw_writer.card_info.LogBlockNbr <= g_raw_writer.cfg.data_start_lba)
    {
        App_Fatal("card too small for raw layout");
    }

    App_RunRawWriterLoop();

    while (1)
    {
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
