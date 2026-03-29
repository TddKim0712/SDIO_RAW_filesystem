#include "raw_diskio.h"
#include "app_sd_test_config.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern SD_HandleTypeDef hsd;

#ifndef APP_RAW_SD_READ_RETRY_COUNT
#define APP_RAW_SD_READ_RETRY_COUNT       1U
#endif

#ifndef APP_RAW_SD_WRITE_RETRY_COUNT
#define APP_RAW_SD_WRITE_RETRY_COUNT      2U
#endif

#ifndef APP_RAW_SD_RECOVERY_DELAY_MS
#define APP_RAW_SD_RECOVERY_DELAY_MS      2U
#endif

#if defined(SDIO_STA_STBITERR)
#define RAW_SD_ALL_IT_MASK ((uint32_t)(SDIO_IT_DATAEND  | SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | \
                                       SDIO_IT_TXUNDERR | SDIO_IT_RXOVERR  | SDIO_IT_TXFIFOHE | \
                                       SDIO_IT_RXFIFOHF | SDIO_IT_STBITERR))
#else
#define RAW_SD_ALL_IT_MASK ((uint32_t)(SDIO_IT_DATAEND  | SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | \
                                       SDIO_IT_TXUNDERR | SDIO_IT_RXOVERR  | SDIO_IT_TXFIFOHE | \
                                       SDIO_IT_RXFIFOHF))
#endif

typedef struct
{
    volatile uint8_t active;
    volatile uint8_t irq_done;
    volatile uint8_t irq_error;
    const void *buf;
    uint32_t start_lba;
    uint32_t block_count;
    uint32_t start_ms;
    uint32_t timeout_ms;
    uint32_t retry_budget;
} raw_sd_async_write_ctx_t;

static uint32_t g_raw_sd_last_error = 0U;
static raw_sd_retry_stats_t g_raw_sd_retry_stats;
static raw_sd_bus_width_t g_raw_sd_transfer_bus_width = RAW_SD_BUS_1BIT;
static uint32_t g_raw_sd_transfer_clock_div = RAW_SD_INIT_CLOCK_DIV_DEFAULT;
static raw_sd_async_write_ctx_t g_raw_sd_async_write;

static int raw_sd_is_word_aligned(const void *ptr)
{
    return ((((uintptr_t)ptr) & 0x3U) == 0U);
}

static void raw_sd_note_status(raw_sd_status_t status)
{
    g_raw_sd_retry_stats.last_status = status;
    g_raw_sd_retry_stats.last_hal_error = g_raw_sd_last_error;
}

static void raw_sd_apply_safe_init_template(void)
{
    hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
    hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
    hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
    hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
    hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd.Init.ClockDiv = RAW_SD_INIT_CLOCK_DIV_DEFAULT;
}

static uint32_t raw_sd_compute_read_timeout(uint32_t block_count)
{
    return (RAW_SD_TIMEOUT_READ_BASE_MS +
            (RAW_SD_TIMEOUT_PER_BLOCK_MS * block_count));
}

static uint32_t raw_sd_compute_write_timeout(uint32_t block_count)
{
    return (RAW_SD_TIMEOUT_WRITE_BASE_MS +
            (RAW_SD_TIMEOUT_PER_BLOCK_MS * block_count));
}

static void raw_sd_async_write_clear(void)
{
    memset(&g_raw_sd_async_write, 0, sizeof(g_raw_sd_async_write));
}

static raw_sd_status_t raw_sd_apply_transfer_clock(uint32_t transfer_clock_div)
{
    if (transfer_clock_div > 0xFFU)
    {
        return RAW_SD_ERR_PARAM;
    }

    if (raw_sd_wait_ready(RAW_SD_TIMEOUT_READY_MS) != RAW_SD_OK)
    {
        return RAW_SD_ERR_TIMEOUT;
    }

    hsd.Init.ClockDiv = transfer_clock_div;

    MODIFY_REG(hsd.Instance->CLKCR,
               SDIO_CLKCR_CLKDIV,
               (transfer_clock_div & SDIO_CLKCR_CLKDIV));

    return RAW_SD_OK;
}

static raw_sd_status_t raw_sd_apply_bus_width(raw_sd_bus_width_t bus_width)
{
    uint32_t hal_bus_width;

    if (bus_width == RAW_SD_BUS_4BIT)
    {
        hal_bus_width = SDIO_BUS_WIDE_4B;
    }
    else
    {
        hal_bus_width = SDIO_BUS_WIDE_1B;
    }

    if (HAL_SD_ConfigWideBusOperation(&hsd, hal_bus_width) != HAL_OK)
    {
        g_raw_sd_last_error = HAL_SD_GetError(&hsd);
        raw_sd_note_status(RAW_SD_ERR_WIDEBUS);
        return RAW_SD_ERR_WIDEBUS;
    }

    hsd.Init.BusWide = hal_bus_width;
    return RAW_SD_OK;
}

static raw_sd_status_t raw_sd_recover_link(void)
{
    raw_sd_status_t status;

    (void)HAL_SD_Abort(&hsd);

    __HAL_SD_DISABLE_IT(&hsd, RAW_SD_ALL_IT_MASK);
    __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_FLAGS);
    hsd.Instance->DCTRL &= (uint32_t)~((uint32_t)(SDIO_DCTRL_DMAEN | SDIO_DCTRL_DTEN));

    if (APP_RAW_SD_RECOVERY_DELAY_MS != 0U)
    {
        HAL_Delay(APP_RAW_SD_RECOVERY_DELAY_MS);
    }

    (void)HAL_SD_DeInit(&hsd);

    raw_sd_apply_safe_init_template();

    if (HAL_SD_Init(&hsd) != HAL_OK)
    {
        g_raw_sd_last_error = HAL_SD_GetError(&hsd);
        raw_sd_note_status(RAW_SD_ERR_INIT);
        return RAW_SD_ERR_INIT;
    }

    g_raw_sd_retry_stats.recover_count++;

    status = raw_sd_reconfigure(g_raw_sd_transfer_bus_width,
                                g_raw_sd_transfer_clock_div);
    if (status != RAW_SD_OK)
    {
        raw_sd_note_status(status);
        return status;
    }

    return RAW_SD_OK;
}

static raw_sd_status_t raw_sd_read_once(uint32_t start_lba,
                                        void *buf,
                                        uint32_t block_count)
{
    uint32_t timeout_ms;

    timeout_ms = raw_sd_compute_read_timeout(block_count);

    if (HAL_SD_ReadBlocks(&hsd,
                          (uint8_t *)buf,
                          start_lba,
                          block_count,
                          timeout_ms) != HAL_OK)
    {
        g_raw_sd_last_error = HAL_SD_GetError(&hsd);
        raw_sd_note_status(RAW_SD_ERR_READ);
        return RAW_SD_ERR_READ;
    }

    return raw_sd_wait_ready(timeout_ms);
}

static raw_sd_status_t raw_sd_write_once(uint32_t start_lba,
                                         const void *buf,
                                         uint32_t block_count)
{
    uint32_t timeout_ms;

    timeout_ms = raw_sd_compute_write_timeout(block_count);

    if (HAL_SD_WriteBlocks(&hsd,
                           (uint8_t *)buf,
                           start_lba,
                           block_count,
                           timeout_ms) != HAL_OK)
    {
        g_raw_sd_last_error = HAL_SD_GetError(&hsd);
        raw_sd_note_status(RAW_SD_ERR_WRITE);
        return RAW_SD_ERR_WRITE;
    }

    return raw_sd_wait_ready(timeout_ms);
}

static raw_sd_status_t raw_sd_async_write_issue(void)
{
    if (HAL_SD_WriteBlocks_DMA(&hsd,
                               (uint8_t *)g_raw_sd_async_write.buf,
                               g_raw_sd_async_write.start_lba,
                               g_raw_sd_async_write.block_count) != HAL_OK)
    {
        g_raw_sd_last_error = HAL_SD_GetError(&hsd);
        raw_sd_note_status(RAW_SD_ERR_WRITE);
        return RAW_SD_ERR_WRITE;
    }

    g_raw_sd_async_write.irq_done = 0U;
    g_raw_sd_async_write.irq_error = 0U;
    g_raw_sd_async_write.start_ms = HAL_GetTick();

    return RAW_SD_IN_PROGRESS;
}

static raw_sd_status_t raw_sd_async_write_retry_or_fail(raw_sd_status_t failed_status)
{
    raw_sd_status_t status;

    status = failed_status;

    for (;;)
    {
        raw_sd_note_status(status);

        if (g_raw_sd_async_write.retry_budget == 0U)
        {
            raw_sd_async_write_clear();
            return status;
        }

        g_raw_sd_async_write.retry_budget--;
        g_raw_sd_retry_stats.write_retry_count++;
        g_raw_sd_retry_stats.async_restart_count++;

        status = raw_sd_recover_link();
        if (status != RAW_SD_OK)
        {
            raw_sd_async_write_clear();
            return status;
        }

        status = raw_sd_async_write_issue();
        if ((status == RAW_SD_IN_PROGRESS) || (status == RAW_SD_OK))
        {
            return status;
        }
    }
}

uint32_t raw_sd_get_last_error(void)
{
    return g_raw_sd_last_error;
}

HAL_SD_CardStateTypeDef raw_sd_get_card_state(void)
{
    return HAL_SD_GetCardState(&hsd);
}

void raw_sd_get_card_info(HAL_SD_CardInfoTypeDef *info)
{
    if (info != NULL)
    {
        HAL_SD_GetCardInfo(&hsd, info);
    }
}

void raw_sd_get_retry_stats(raw_sd_retry_stats_t *stats)
{
    if (stats != NULL)
    {
        *stats = g_raw_sd_retry_stats;
    }
}

void raw_sd_reset_retry_stats(void)
{
    memset(&g_raw_sd_retry_stats, 0, sizeof(g_raw_sd_retry_stats));
}

raw_sd_status_t raw_sd_wait_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER)
        {
            return RAW_SD_OK;
        }

        HAL_Delay(1U);
    }

    g_raw_sd_last_error = HAL_SD_GetError(&hsd);
    raw_sd_note_status(RAW_SD_ERR_TIMEOUT);
    return RAW_SD_ERR_TIMEOUT;
}

raw_sd_status_t raw_sd_init(raw_sd_bus_width_t bus_width,
                            uint32_t transfer_clock_div)
{
    raw_sd_apply_safe_init_template();

    if (HAL_SD_Init(&hsd) != HAL_OK)
    {
        g_raw_sd_last_error = HAL_SD_GetError(&hsd);
        raw_sd_note_status(RAW_SD_ERR_INIT);
        return RAW_SD_ERR_INIT;
    }

    return raw_sd_reconfigure(bus_width, transfer_clock_div);
}

raw_sd_status_t raw_sd_reconfigure(raw_sd_bus_width_t bus_width,
                                   uint32_t transfer_clock_div)
{
    raw_sd_status_t status;

    status = raw_sd_wait_ready(RAW_SD_TIMEOUT_READY_MS);
    if (status != RAW_SD_OK)
    {
        return status;
    }

    status = raw_sd_apply_bus_width(bus_width);
    if (status != RAW_SD_OK)
    {
        return status;
    }

    status = raw_sd_apply_transfer_clock(transfer_clock_div);
    if (status != RAW_SD_OK)
    {
        return status;
    }

    status = raw_sd_wait_ready(RAW_SD_TIMEOUT_READY_MS);
    if (status != RAW_SD_OK)
    {
        return status;
    }

    g_raw_sd_transfer_bus_width = bus_width;
    g_raw_sd_transfer_clock_div = transfer_clock_div;

    return RAW_SD_OK;
}

raw_sd_status_t raw_sd_read_blocks(uint32_t start_lba,
                                   void *buf,
                                   uint32_t block_count)
{
    raw_sd_status_t status;
    uint32_t attempt;

    if ((buf == NULL) || (block_count == 0U))
    {
        return RAW_SD_ERR_PARAM;
    }

    if (!raw_sd_is_word_aligned(buf))
    {
        return RAW_SD_ERR_ALIGN;
    }

    for (attempt = 0U; ; attempt++)
    {
        status = raw_sd_read_once(start_lba, buf, block_count);
        if (status == RAW_SD_OK)
        {
            return RAW_SD_OK;
        }

        if (attempt >= APP_RAW_SD_READ_RETRY_COUNT)
        {
            return status;
        }

        g_raw_sd_retry_stats.read_retry_count++;

        status = raw_sd_recover_link();
        if (status != RAW_SD_OK)
        {
            return status;
        }
    }
}

raw_sd_status_t raw_sd_write_blocks(uint32_t start_lba,
                                    const void *buf,
                                    uint32_t block_count)
{
    raw_sd_status_t status;
    uint32_t attempt;

    if ((buf == NULL) || (block_count == 0U))
    {
        return RAW_SD_ERR_PARAM;
    }

    if (!raw_sd_is_word_aligned(buf))
    {
        return RAW_SD_ERR_ALIGN;
    }

    for (attempt = 0U; ; attempt++)
    {
        status = raw_sd_write_once(start_lba, buf, block_count);
        if (status == RAW_SD_OK)
        {
            return RAW_SD_OK;
        }

        if (attempt >= APP_RAW_SD_WRITE_RETRY_COUNT)
        {
            return status;
        }

        g_raw_sd_retry_stats.write_retry_count++;

        status = raw_sd_recover_link();
        if (status != RAW_SD_OK)
        {
            return status;
        }
    }
}

raw_sd_status_t raw_sd_write_blocks_async_start(uint32_t start_lba,
                                                const void *buf,
                                                uint32_t block_count)
{
    if ((buf == NULL) || (block_count == 0U))
    {
        return RAW_SD_ERR_PARAM;
    }

    if (!raw_sd_is_word_aligned(buf))
    {
        return RAW_SD_ERR_ALIGN;
    }

    if (g_raw_sd_async_write.active != 0U)
    {
        raw_sd_note_status(RAW_SD_ERR_STATE);
        return RAW_SD_ERR_STATE;
    }

    raw_sd_async_write_clear();

    g_raw_sd_async_write.active = 1U;
    g_raw_sd_async_write.buf = buf;
    g_raw_sd_async_write.start_lba = start_lba;
    g_raw_sd_async_write.block_count = block_count;
    g_raw_sd_async_write.timeout_ms = raw_sd_compute_write_timeout(block_count);
    g_raw_sd_async_write.retry_budget = APP_RAW_SD_WRITE_RETRY_COUNT;

    {
        raw_sd_status_t status;

        status = raw_sd_async_write_issue();
        if ((status != RAW_SD_IN_PROGRESS) && (status != RAW_SD_OK))
        {
            return raw_sd_async_write_retry_or_fail(status);
        }

        return status;
    }
}

raw_sd_status_t raw_sd_write_blocks_async_service(void)
{
    uint32_t elapsed_ms;

    if (g_raw_sd_async_write.active == 0U)
    {
        return RAW_SD_OK;
    }

    if (g_raw_sd_async_write.irq_error != 0U)
    {
        return raw_sd_async_write_retry_or_fail(RAW_SD_ERR_WRITE);
    }

    elapsed_ms = HAL_GetTick() - g_raw_sd_async_write.start_ms;

    if (g_raw_sd_async_write.irq_done == 0U)
    {
        if (elapsed_ms < g_raw_sd_async_write.timeout_ms)
        {
            return RAW_SD_IN_PROGRESS;
        }

        return raw_sd_async_write_retry_or_fail(RAW_SD_ERR_TIMEOUT);
    }

    if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER)
    {
        raw_sd_async_write_clear();
        return RAW_SD_OK;
    }

    if (elapsed_ms < g_raw_sd_async_write.timeout_ms)
    {
        return RAW_SD_IN_PROGRESS;
    }

    return raw_sd_async_write_retry_or_fail(RAW_SD_ERR_TIMEOUT);
}

int raw_sd_write_blocks_async_busy(void)
{
    return ((g_raw_sd_async_write.active != 0U) ? 1 : 0);
}

void HAL_SD_TxCpltCallback(SD_HandleTypeDef *sd_handle)
{
    if ((sd_handle == &hsd) && (g_raw_sd_async_write.active != 0U))
    {
        g_raw_sd_async_write.irq_done = 1U;
    }
}

void HAL_SD_RxCpltCallback(SD_HandleTypeDef *sd_handle)
{
    (void)sd_handle;
}

void HAL_SD_ErrorCallback(SD_HandleTypeDef *sd_handle)
{
    if (sd_handle != &hsd)
    {
        return;
    }

    g_raw_sd_last_error = HAL_SD_GetError(sd_handle);

    if (g_raw_sd_async_write.active != 0U)
    {
        g_raw_sd_async_write.irq_error = 1U;
    }
}
