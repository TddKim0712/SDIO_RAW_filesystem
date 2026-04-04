#ifndef RAW_LOG_WRITER_H
#define RAW_LOG_WRITER_H

#include "raw_diskio.h"
#include "raw_log_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RAW_LOG_WRITER_ASYNC_MAX_BLOCKS
#define RAW_LOG_WRITER_ASYNC_MAX_BLOCKS RAW_LOG_RECOMMENDED_BURST_BLOCKS
#endif

typedef enum
{
    RAW_LOG_WRITER_OK = 0,
    RAW_LOG_WRITER_IN_PROGRESS = 1,
    RAW_LOG_WRITER_ERR_PARAM = -1,
    RAW_LOG_WRITER_ERR_LAYOUT = -2,
    RAW_LOG_WRITER_ERR_SD_READ = -3,
    RAW_LOG_WRITER_ERR_SD_WRITE = -4,
    RAW_LOG_WRITER_ERR_SD_INFO = -5,
    RAW_LOG_WRITER_ERR_RECOVERY = -6,
    RAW_LOG_WRITER_ERR_STATE = -7
} raw_log_writer_result_t;

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
} raw_log_writer_step_info_t;

typedef enum
{
    RAW_LOG_WRITER_ASYNC_IDLE = 0,
    RAW_LOG_WRITER_ASYNC_DATA_BUSY = 1,
    RAW_LOG_WRITER_ASYNC_SUPERBLOCK_BUSY = 2
} raw_log_writer_async_state_t;

typedef struct
{
    raw_log_writer_async_state_t state;
    raw_log_state_t checkpoint_state;
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
} raw_log_writer_async_ctx_t;

typedef struct
{
    raw_log_config_t cfg;
    raw_log_state_t  state;
    HAL_SD_CardInfoTypeDef card_info;

    uint32_t tx_block[RAW_SD_BLOCK_SIZE / sizeof(uint32_t)];
    uint32_t tx_burst_blocks[(RAW_LOG_WRITER_ASYNC_MAX_BLOCKS * RAW_SD_BLOCK_SIZE) /
                             sizeof(uint32_t)];
    uint32_t rx_block[RAW_SD_BLOCK_SIZE / sizeof(uint32_t)];

    uint32_t log_every_n_blocks;
    uint32_t payload_flags;
    raw_log_writer_async_ctx_t async;
} raw_log_writer_t;

void raw_log_writer_init_defaults(raw_log_writer_t *writer);
void raw_log_writer_set_payload_flags(raw_log_writer_t *writer,
                                      uint32_t payload_flags);

raw_log_writer_result_t raw_log_writer_begin(raw_log_writer_t *writer);

raw_log_writer_result_t raw_log_writer_write_payload(raw_log_writer_t *writer,
                                                     const void *payload,
                                                     uint32_t payload_bytes,
                                                     raw_log_writer_step_info_t *step_info);

int raw_log_writer_async_busy(const raw_log_writer_t *writer);

uint32_t raw_log_writer_get_max_contiguous_write_blocks(const raw_log_writer_t *writer);

raw_log_writer_result_t raw_log_writer_start_payloads_async(raw_log_writer_t *writer,
                                                            const void **payloads,
                                                            const uint32_t *payload_bytes,
                                                            uint32_t block_count);

raw_log_writer_result_t raw_log_writer_start_payload_async(raw_log_writer_t *writer,
                                                           const void *payload,
                                                           uint32_t payload_bytes);

raw_log_writer_result_t raw_log_writer_service_async(raw_log_writer_t *writer,
                                                     raw_log_writer_step_info_t *step_info);

void raw_log_writer_fill_test_payload(void *payload,
                                      uint32_t payload_bytes,
                                      uint32_t seq,
                                      uint32_t lba);

#ifdef __cplusplus
}
#endif

#endif /* RAW_LOG_WRITER_H */
