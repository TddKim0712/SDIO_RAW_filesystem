#include "sdrec_card_port.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern SD_HandleTypeDef hsd;


#if defined(SDIO_STA_STBITERR)
#define SDREC_CARD_ALL_IT_MASK ((uint32_t)(SDIO_IT_DATAEND  | SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | \
                                       SDIO_IT_TXUNDERR | SDIO_IT_RXOVERR  | SDIO_IT_TXFIFOHE | \
                                       SDIO_IT_RXFIFOHF | SDIO_IT_STBITERR))
#else
#define SDREC_CARD_ALL_IT_MASK ((uint32_t)(SDIO_IT_DATAEND  | SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT | \
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
} sdrec_card_async_write_ctx_t;

static uint32_t g_sdrec_card_last_error = 0U;
static sdrec_card_retry_stats_t g_sdrec_card_retry_stats;
static sdrec_card_policy_t g_sdrec_card_policy;
static sdrec_card_bus_width_t g_sdrec_card_transfer_bus_width = SDREC_CARD_BUS_1BIT;
static uint32_t g_sdrec_card_transfer_clock_div = SDREC_CARD_INIT_CLOCK_DIV_DEFAULT;
static sdrec_card_async_write_ctx_t g_sdrec_card_async_write;

static int sdrec_card_is_word_aligned(const void *ptr)
{
    return ((((uintptr_t)ptr) & 0x3U) == 0U);
}

static void sdrec_card_note_status(sdrec_card_status_t status)
{
    g_sdrec_card_retry_stats.last_status = status;
    g_sdrec_card_retry_stats.last_hal_error = g_sdrec_card_last_error;
}

static void sdrec_card_apply_safe_init_template(void)
{
    hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
    hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
    hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
    hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
    hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd.Init.ClockDiv = SDREC_CARD_INIT_CLOCK_DIV_DEFAULT;
}

static uint32_t sdrec_card_compute_read_timeout(uint32_t block_count)
{
    return (SDREC_CARD_TIMEOUT_READ_BASE_MS +
            (SDREC_CARD_TIMEOUT_PER_BLOCK_MS * block_count));
}

static uint32_t sdrec_card_compute_write_timeout(uint32_t block_count)
{
    return (SDREC_CARD_TIMEOUT_WRITE_BASE_MS +
            (SDREC_CARD_TIMEOUT_PER_BLOCK_MS * block_count));
}

static void sdrec_card_async_write_clear(void)
{
    memset(&g_sdrec_card_async_write, 0, sizeof(g_sdrec_card_async_write));
}

static sdrec_card_status_t sdrec_card_apply_transfer_clock(uint32_t transfer_clock_div)
{
    if (transfer_clock_div > 0xFFU)
    {
        return SDREC_CARD_ERR_PARAM;
    }

    if (sdrec_card_wait_ready(SDREC_CARD_TIMEOUT_READY_MS) != SDREC_CARD_OK)
    {
        return SDREC_CARD_ERR_TIMEOUT;
    }

    hsd.Init.ClockDiv = transfer_clock_div;

    MODIFY_REG(hsd.Instance->CLKCR,
               SDIO_CLKCR_CLKDIV,
               (transfer_clock_div & SDIO_CLKCR_CLKDIV));

    return SDREC_CARD_OK;
}

static sdrec_card_status_t sdrec_card_apply_bus_width(sdrec_card_bus_width_t bus_width)
{
    uint32_t hal_bus_width;

    if (bus_width == SDREC_CARD_BUS_4BIT)
    {
        hal_bus_width = SDIO_BUS_WIDE_4B;
    }
    else
    {
        hal_bus_width = SDIO_BUS_WIDE_1B;
    }

    if (HAL_SD_ConfigWideBusOperation(&hsd, hal_bus_width) != HAL_OK)
    {
        g_sdrec_card_last_error = HAL_SD_GetError(&hsd);
        sdrec_card_note_status(SDREC_CARD_ERR_WIDEBUS);
        return SDREC_CARD_ERR_WIDEBUS;
    }

    hsd.Init.BusWide = hal_bus_width;
    return SDREC_CARD_OK;
}

static sdrec_card_status_t sdrec_card_recover_link(void)
{
    sdrec_card_status_t status;

    (void)HAL_SD_Abort(&hsd);

    __HAL_SD_DISABLE_IT(&hsd, SDREC_CARD_ALL_IT_MASK);
    __HAL_SD_CLEAR_FLAG(&hsd, SDIO_STATIC_FLAGS);
    hsd.Instance->DCTRL &= (uint32_t)~((uint32_t)(SDIO_DCTRL_DMAEN | SDIO_DCTRL_DTEN));

    if (g_sdrec_card_policy.recovery_delay_ms != 0U)
    {
        HAL_Delay(g_sdrec_card_policy.recovery_delay_ms);
    }

    (void)HAL_SD_DeInit(&hsd);

    sdrec_card_apply_safe_init_template();

    if (HAL_SD_Init(&hsd) != HAL_OK)
    {
        g_sdrec_card_last_error = HAL_SD_GetError(&hsd);
        sdrec_card_note_status(SDREC_CARD_ERR_INIT);
        return SDREC_CARD_ERR_INIT;
    }

    g_sdrec_card_retry_stats.recover_count++;

    status = sdrec_card_reconfigure(g_sdrec_card_transfer_bus_width,
                                g_sdrec_card_transfer_clock_div);
    if (status != SDREC_CARD_OK)
    {
        sdrec_card_note_status(status);
        return status;
    }

    return SDREC_CARD_OK;
}

static sdrec_card_status_t sdrec_card_read_once(uint32_t start_lba,
                                        void *buf,
                                        uint32_t block_count)
{
    uint32_t timeout_ms;

    timeout_ms = sdrec_card_compute_read_timeout(block_count);

    if (HAL_SD_ReadBlocks(&hsd,
                          (uint8_t *)buf,
                          start_lba,
                          block_count,
                          timeout_ms) != HAL_OK)
    {
        g_sdrec_card_last_error = HAL_SD_GetError(&hsd);
        sdrec_card_note_status(SDREC_CARD_ERR_READ);
        return SDREC_CARD_ERR_READ;
    }

    return sdrec_card_wait_ready(timeout_ms);
}

static sdrec_card_status_t sdrec_card_write_once(uint32_t start_lba,
                                         const void *buf,
                                         uint32_t block_count)
{
    uint32_t timeout_ms;

    timeout_ms = sdrec_card_compute_write_timeout(block_count);

    if (HAL_SD_WriteBlocks(&hsd,
                           (uint8_t *)buf,
                           start_lba,
                           block_count,
                           timeout_ms) != HAL_OK)
    {
        g_sdrec_card_last_error = HAL_SD_GetError(&hsd);
        sdrec_card_note_status(SDREC_CARD_ERR_WRITE);
        return SDREC_CARD_ERR_WRITE;
    }

    return sdrec_card_wait_ready(timeout_ms);
}

static sdrec_card_status_t sdrec_card_async_write_issue(void)
{
    g_sdrec_card_async_write.irq_done = 0U;
    g_sdrec_card_async_write.irq_error = 0U;
    g_sdrec_card_async_write.start_ms = HAL_GetTick();

    if (HAL_SD_WriteBlocks_DMA(&hsd,
                               (uint8_t *)g_sdrec_card_async_write.buf,
                               g_sdrec_card_async_write.start_lba,
                               g_sdrec_card_async_write.block_count) != HAL_OK)
    {
        g_sdrec_card_last_error = HAL_SD_GetError(&hsd);
        sdrec_card_note_status(SDREC_CARD_ERR_WRITE);
        return SDREC_CARD_ERR_WRITE;
    }

    return SDREC_CARD_IN_PROGRESS;
}

static sdrec_card_status_t sdrec_card_async_write_retry_or_fail(sdrec_card_status_t failed_status)
{
    sdrec_card_status_t status;

    status = failed_status;

    for (;;)
    {
        sdrec_card_note_status(status);

        if (g_sdrec_card_async_write.retry_budget == 0U)
        {
            sdrec_card_async_write_clear();
            return status;
        }

        g_sdrec_card_async_write.retry_budget--;
        g_sdrec_card_retry_stats.write_retry_count++;
        g_sdrec_card_retry_stats.async_restart_count++;

        status = sdrec_card_recover_link();
        if (status != SDREC_CARD_OK)
        {
            sdrec_card_async_write_clear();
            return status;
        }

        status = sdrec_card_async_write_issue();
        if ((status == SDREC_CARD_IN_PROGRESS) || (status == SDREC_CARD_OK))
        {
            return status;
        }
    }
}

void sdrec_card_policy_init_defaults(sdrec_card_policy_t *policy)
{
    if (policy == NULL)
    {
        return;
    }

    policy->read_retry_count = 1U;
    policy->write_retry_count = 2U;
    policy->recovery_delay_ms = 2U;
}

void sdrec_card_set_policy(const sdrec_card_policy_t *policy)
{
    if (policy == NULL)
    {
        sdrec_card_policy_init_defaults(&g_sdrec_card_policy);
        return;
    }

    g_sdrec_card_policy = *policy;
}

void sdrec_card_get_policy(sdrec_card_policy_t *policy)
{
    if (policy == NULL)
    {
        return;
    }

    *policy = g_sdrec_card_policy;
}

uint32_t sdrec_card_get_last_error(void)
{
    return g_sdrec_card_last_error;
}

HAL_SD_CardStateTypeDef sdrec_card_get_card_state(void)
{
    return HAL_SD_GetCardState(&hsd);
}

void sdrec_card_get_card_info(HAL_SD_CardInfoTypeDef *info)
{
    if (info != NULL)
    {
        HAL_SD_GetCardInfo(&hsd, info);
    }
}

void sdrec_card_get_retry_stats(sdrec_card_retry_stats_t *stats)
{
    if (stats != NULL)
    {
        *stats = g_sdrec_card_retry_stats;
    }
}

void sdrec_card_reset_retry_stats(void)
{
    memset(&g_sdrec_card_retry_stats, 0, sizeof(g_sdrec_card_retry_stats));
}

sdrec_card_status_t sdrec_card_wait_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER)
        {
            return SDREC_CARD_OK;
        }

        HAL_Delay(1U);
    }

    g_sdrec_card_last_error = HAL_SD_GetError(&hsd);
    sdrec_card_note_status(SDREC_CARD_ERR_TIMEOUT);
    return SDREC_CARD_ERR_TIMEOUT;
}

sdrec_card_status_t sdrec_card_init(sdrec_card_bus_width_t bus_width,
                            uint32_t transfer_clock_div)
{
    if ((g_sdrec_card_policy.read_retry_count == 0U) &&
        (g_sdrec_card_policy.write_retry_count == 0U) &&
        (g_sdrec_card_policy.recovery_delay_ms == 0U))
    {
        sdrec_card_policy_init_defaults(&g_sdrec_card_policy);
    }

    sdrec_card_apply_safe_init_template();

    if (HAL_SD_Init(&hsd) != HAL_OK)
    {
        g_sdrec_card_last_error = HAL_SD_GetError(&hsd);
        sdrec_card_note_status(SDREC_CARD_ERR_INIT);
        return SDREC_CARD_ERR_INIT;
    }

    return sdrec_card_reconfigure(bus_width, transfer_clock_div);
}

sdrec_card_status_t sdrec_card_reconfigure(sdrec_card_bus_width_t bus_width,
                                   uint32_t transfer_clock_div)
{
    sdrec_card_status_t status;

    status = sdrec_card_wait_ready(SDREC_CARD_TIMEOUT_READY_MS);
    if (status != SDREC_CARD_OK)
    {
        return status;
    }

    status = sdrec_card_apply_bus_width(bus_width);
    if (status != SDREC_CARD_OK)
    {
        return status;
    }

    status = sdrec_card_apply_transfer_clock(transfer_clock_div);
    if (status != SDREC_CARD_OK)
    {
        return status;
    }

    status = sdrec_card_wait_ready(SDREC_CARD_TIMEOUT_READY_MS);
    if (status != SDREC_CARD_OK)
    {
        return status;
    }

    g_sdrec_card_transfer_bus_width = bus_width;
    g_sdrec_card_transfer_clock_div = transfer_clock_div;

    return SDREC_CARD_OK;
}

sdrec_card_status_t sdrec_card_read_blocks(uint32_t start_lba,
                                   void *buf,
                                   uint32_t block_count)
{
    sdrec_card_status_t status;
    uint32_t attempt;

    if ((buf == NULL) || (block_count == 0U))
    {
        return SDREC_CARD_ERR_PARAM;
    }

    if (!sdrec_card_is_word_aligned(buf))
    {
        return SDREC_CARD_ERR_ALIGN;
    }

    for (attempt = 0U; ; attempt++)
    {
        status = sdrec_card_read_once(start_lba, buf, block_count);
        if (status == SDREC_CARD_OK)
        {
            return SDREC_CARD_OK;
        }

        if (attempt >= g_sdrec_card_policy.read_retry_count)
        {
            return status;
        }

        g_sdrec_card_retry_stats.read_retry_count++;

        status = sdrec_card_recover_link();
        if (status != SDREC_CARD_OK)
        {
            return status;
        }
    }
}

sdrec_card_status_t sdrec_card_write_blocks(uint32_t start_lba,
                                    const void *buf,
                                    uint32_t block_count)
{
    sdrec_card_status_t status;
    uint32_t attempt;

    if ((buf == NULL) || (block_count == 0U))
    {
        return SDREC_CARD_ERR_PARAM;
    }

    if (!sdrec_card_is_word_aligned(buf))
    {
        return SDREC_CARD_ERR_ALIGN;
    }

    for (attempt = 0U; ; attempt++)
    {
        status = sdrec_card_write_once(start_lba, buf, block_count);
        if (status == SDREC_CARD_OK)
        {
            return SDREC_CARD_OK;
        }

        if (attempt >= g_sdrec_card_policy.write_retry_count)
        {
            return status;
        }

        g_sdrec_card_retry_stats.write_retry_count++;

        status = sdrec_card_recover_link();
        if (status != SDREC_CARD_OK)
        {
            return status;
        }
    }
}

sdrec_card_status_t sdrec_card_write_async_begin(uint32_t start_lba,
                                                const void *buf,
                                                uint32_t block_count)
{
    if ((buf == NULL) || (block_count == 0U))
    {
        return SDREC_CARD_ERR_PARAM;
    }

    if (!sdrec_card_is_word_aligned(buf))
    {
        return SDREC_CARD_ERR_ALIGN;
    }

    if (g_sdrec_card_async_write.active != 0U)
    {
        sdrec_card_note_status(SDREC_CARD_ERR_STATE);
        return SDREC_CARD_ERR_STATE;
    }

    sdrec_card_async_write_clear();

    g_sdrec_card_async_write.active = 1U;
    g_sdrec_card_async_write.buf = buf;
    g_sdrec_card_async_write.start_lba = start_lba;
    g_sdrec_card_async_write.block_count = block_count;
    g_sdrec_card_async_write.timeout_ms = sdrec_card_compute_write_timeout(block_count);
    g_sdrec_card_async_write.retry_budget = g_sdrec_card_policy.write_retry_count;

    {
        sdrec_card_status_t status;

        status = sdrec_card_async_write_issue();
        if ((status != SDREC_CARD_IN_PROGRESS) && (status != SDREC_CARD_OK))
        {
            return sdrec_card_async_write_retry_or_fail(status);
        }

        return status;
    }
}

sdrec_card_status_t sdrec_card_write_async_poll(void)
{
    uint32_t elapsed_ms;

    if (g_sdrec_card_async_write.active == 0U)
    {
        return SDREC_CARD_OK;
    }

    if (g_sdrec_card_async_write.irq_error != 0U)
    {
        return sdrec_card_async_write_retry_or_fail(SDREC_CARD_ERR_WRITE);
    }

    elapsed_ms = HAL_GetTick() - g_sdrec_card_async_write.start_ms;

    if (g_sdrec_card_async_write.irq_done == 0U)
    {
        if (elapsed_ms < g_sdrec_card_async_write.timeout_ms)
        {
            return SDREC_CARD_IN_PROGRESS;
        }

        return sdrec_card_async_write_retry_or_fail(SDREC_CARD_ERR_TIMEOUT);
    }

    if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER)
    {
        sdrec_card_async_write_clear();
        return SDREC_CARD_OK;
    }

    if (elapsed_ms < g_sdrec_card_async_write.timeout_ms)
    {
        return SDREC_CARD_IN_PROGRESS;
    }

    return sdrec_card_async_write_retry_or_fail(SDREC_CARD_ERR_TIMEOUT);
}

int sdrec_card_write_async_busy(void)
{
    return ((g_sdrec_card_async_write.active != 0U) ? 1 : 0);
}

void HAL_SD_TxCpltCallback(SD_HandleTypeDef *sd_handle)
{
    if ((sd_handle == &hsd) && (g_sdrec_card_async_write.active != 0U))
    {
        g_sdrec_card_async_write.irq_done = 1U;
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

    g_sdrec_card_last_error = HAL_SD_GetError(sd_handle);

    if (g_sdrec_card_async_write.active != 0U)
    {
        g_sdrec_card_async_write.irq_error = 1U;
    }
}
