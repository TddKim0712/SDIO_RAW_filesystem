#ifndef APP_SDREC_PROFILE_H
#define APP_SDREC_PROFILE_H

#include "sdrec_runtime.h"
#include "sdrec_source_dummy_cpu.h"
#include "sdrec_source_wave_table.h"
#include "sdrec_source_wave_dma.h"

#define APP_ENABLE_UART_LOG                 1U

#define APP_SDREC_USE_EXTERNAL_SOURCE       0U
#define APP_SDREC_USE_DUMMY_CPU_SOURCE      0U
#define APP_SDREC_USE_WAVE_TABLE_SOURCE     1U
#define APP_SDREC_USE_WAVE_DMA_SOURCE       0U

#define APP_SDREC_CARD_BUS_WIDTH            SDREC_CARD_BUS_4BIT
#define APP_SDREC_CARD_CLOCK_DIV            5U
#define APP_SDREC_LOG_EVERY_N_BLOCKS        64U

#define APP_SDREC_RING_START_LBA            0U
#define APP_SDREC_RING_COUNT                32U
#define APP_SDREC_DATA_START_LBA            32U
#define APP_SDREC_SUPERBLOCK_INTERVAL       16U
#define APP_SDREC_STALL_THRESHOLD_MS        20U

#define APP_SDREC_READ_RETRY_COUNT          1U
#define APP_SDREC_WRITE_RETRY_COUNT         2U
#define APP_SDREC_RECOVERY_DELAY_MS         2U

#define APP_SDREC_DUMMY_PERIOD_MS           1U
#define APP_SDREC_WAVE_PERIOD_MS            1U

static inline void AppSdrec_FillRuntimeCfg(sdrec_runtime_cfg_t *cfg)
{
    sdrec_runtime_cfg_init_defaults(cfg);
    cfg->bus_width = APP_SDREC_CARD_BUS_WIDTH;
    cfg->transfer_clock_div = APP_SDREC_CARD_CLOCK_DIV;
    cfg->log_every_n_blocks = APP_SDREC_LOG_EVERY_N_BLOCKS;

    cfg->layout_cfg.superblock_ring_start_lba = APP_SDREC_RING_START_LBA;
    cfg->layout_cfg.superblock_ring_count = APP_SDREC_RING_COUNT;
    cfg->layout_cfg.data_start_lba = APP_SDREC_DATA_START_LBA;
    cfg->layout_cfg.superblock_write_interval = APP_SDREC_SUPERBLOCK_INTERVAL;
    cfg->layout_cfg.stall_threshold_ms = APP_SDREC_STALL_THRESHOLD_MS;

    cfg->sd_policy.read_retry_count = APP_SDREC_READ_RETRY_COUNT;
    cfg->sd_policy.write_retry_count = APP_SDREC_WRITE_RETRY_COUNT;
    cfg->sd_policy.recovery_delay_ms = APP_SDREC_RECOVERY_DELAY_MS;
}

static inline void AppSdrec_ConfigDummy(sdrec_dummy_cpu_source_t *dummy_src)
{
    sdrec_dummy_cpu_source_init_defaults(dummy_src);
    dummy_src->period_ms = APP_SDREC_DUMMY_PERIOD_MS;
}

static inline void AppSdrec_ConfigWaveTable(sdrec_wave_table_source_t *wave_src)
{
    sdrec_wave_table_source_init_defaults(wave_src);
    wave_src->period_ms = APP_SDREC_WAVE_PERIOD_MS;
}

static inline void AppSdrec_ConfigWaveDma(sdrec_wave_dma_source_t *wave_dma_src)
{
    sdrec_wave_dma_source_init_defaults(wave_dma_src);
    wave_dma_src->table.period_ms = APP_SDREC_WAVE_PERIOD_MS;
}

#endif
