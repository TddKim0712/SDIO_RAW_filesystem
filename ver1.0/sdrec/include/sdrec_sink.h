#ifndef SDREC_SINK_H
#define SDREC_SINK_H

#include "sdrec_card_port.h"
#include "sdrec_layout_v3.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SDREC_SINK_MAX_ASYNC_BLOCKS
#define SDREC_SINK_MAX_ASYNC_BLOCKS SDREC_RECOMMENDED_BURST_BLOCKS
#endif

typedef enum
{
    SDREC_SINK_OK = 0,
    SDREC_SINK_IN_PROGRESS = 1,
    SDREC_SINK_ERR_PARAM = -1,
    SDREC_SINK_ERR_LAYOUT = -2,
    SDREC_SINK_ERR_CARD_READ = -3,
    SDREC_SINK_ERR_CARD_WRITE = -4,
    SDREC_SINK_ERR_CARD_INFO = -5,
    SDREC_SINK_ERR_RECOVERY = -6,
    SDREC_SINK_ERR_STATE = -7
} sdrec_sink_status_t;

typedef struct
{
    uint32_t seq;
    uint32_t last_seq;
    uint32_t block_count;
    uint32_t data_lba;
    uint32_t next_data_lba;
    uint32_t superblock_lba;
    uint32_t data_ms;
    uint32_t superblock_ms;
    uint32_t total_ms;
    uint32_t stall_count;
    uint8_t  superblock_written;
    uint8_t  stall;
} sdrec_commit_report_t;

typedef enum
{
    SDREC_SINK_ASYNC_IDLE = 0,
    SDREC_SINK_ASYNC_DATA_BUSY = 1,
    SDREC_SINK_ASYNC_SUPERBLOCK_BUSY = 2
} sdrec_sink_async_state_t;

typedef struct
{
    sdrec_sink_async_state_t state;
    sdrec_layout_state_t checkpoint_state;
    uint32_t data_lba;
    uint32_t last_data_lba;
    uint32_t first_seq;
    uint32_t last_seq;
    uint32_t block_count;
    uint32_t superblock_lba;
    uint32_t tick_ms;
    uint32_t data_start_ms;
    uint32_t data_ms;
    uint32_t superblock_start_ms;
    uint32_t superblock_ms;
    uint8_t  superblock_needed;
} sdrec_sink_async_ctx_t;

typedef struct
{
    sdrec_layout_cfg_t layout_cfg;
    sdrec_layout_state_t layout_state;
    HAL_SD_CardInfoTypeDef card_info;

    uint32_t tx_block[SDREC_CARD_BLOCK_SIZE / sizeof(uint32_t)];
    uint32_t tx_burst_blocks[(SDREC_SINK_MAX_ASYNC_BLOCKS * SDREC_CARD_BLOCK_SIZE) /
                             sizeof(uint32_t)];
    uint32_t rx_block[SDREC_CARD_BLOCK_SIZE / sizeof(uint32_t)];

    uint32_t log_every_n_blocks;
    uint32_t payload_flags;
    sdrec_sink_async_ctx_t async;
} sdrec_sink_t;

void sdrec_sink_init_defaults(sdrec_sink_t *sink);
void sdrec_sink_set_payload_flags(sdrec_sink_t *sink,
                                      uint32_t payload_flags);

void sdrec_sink_set_runtime_config(sdrec_sink_t *sink,
                                       const sdrec_layout_cfg_t *cfg,
                                       uint32_t log_every_n_blocks,
                                       uint32_t payload_flags);

sdrec_sink_status_t sdrec_sink_open(sdrec_sink_t *sink);

sdrec_sink_status_t sdrec_sink_write_payload(sdrec_sink_t *sink,
                                                     const void *payload,
                                                     uint32_t payload_bytes,
                                                     sdrec_commit_report_t *step_info);

int sdrec_sink_async_busy(const sdrec_sink_t *sink);

uint32_t sdrec_sink_get_max_contiguous_write_blocks(const sdrec_sink_t *sink);

sdrec_sink_status_t sdrec_sink_begin_async_batch(sdrec_sink_t *sink,
                                                            const void **payloads,
                                                            const uint32_t *payload_bytes,
                                                            uint32_t block_count);

sdrec_sink_status_t sdrec_sink_begin_async_single(sdrec_sink_t *sink,
                                                           const void *payload,
                                                           uint32_t payload_bytes);

sdrec_sink_status_t sdrec_sink_poll_async(sdrec_sink_t *sink,
                                                     sdrec_commit_report_t *step_info);

void sdrec_sink_fill_test_payload(void *payload,
                                      uint32_t payload_bytes,
                                      uint32_t seq,
                                      uint32_t lba);

#ifdef __cplusplus
}
#endif

#endif /* SDREC_SINK_H */
