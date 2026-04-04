#ifndef RAW_LOG_STREAM_H
#define RAW_LOG_STREAM_H

#include "raw_log_writer.h"
#include "app_sd_test_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef APP_RAW_LOG_STREAM_SLOT_COUNT
#define APP_RAW_LOG_STREAM_SLOT_COUNT 4U
#endif

#ifndef RAW_LOG_STREAM_SLOT_COUNT
#define RAW_LOG_STREAM_SLOT_COUNT APP_RAW_LOG_STREAM_SLOT_COUNT
#endif

#ifndef RAW_LOG_STREAM_DMA_BURST_MAX_BLOCKS
#if (RAW_LOG_STREAM_SLOT_COUNT < RAW_LOG_WRITER_ASYNC_MAX_BLOCKS)
#define RAW_LOG_STREAM_DMA_BURST_MAX_BLOCKS RAW_LOG_STREAM_SLOT_COUNT
#else
#define RAW_LOG_STREAM_DMA_BURST_MAX_BLOCKS RAW_LOG_WRITER_ASYNC_MAX_BLOCKS
#endif
#endif

typedef enum
{
    RAW_LOG_STREAM_SLOT_FREE = 0,
    RAW_LOG_STREAM_SLOT_FILLING = 1,
    RAW_LOG_STREAM_SLOT_READY = 2,
    RAW_LOG_STREAM_SLOT_INFLIGHT = 3
} raw_log_stream_slot_state_t;

typedef union
{
    uint32_t words[(RAW_LOG_DATA_PAYLOAD_BYTES_V3 + sizeof(uint32_t) - 1U) / sizeof(uint32_t)];
    uint8_t  bytes[RAW_LOG_DATA_PAYLOAD_BYTES_V3];
} raw_log_stream_payload_t;

typedef struct
{
    raw_log_stream_payload_t payload;
    uint32_t payload_bytes;
    uint32_t produced_tick_ms;
    uint8_t  state;
    uint8_t  reserved[3];
} raw_log_stream_slot_t;

typedef struct
{
    raw_log_writer_t writer;
    raw_log_stream_slot_t slots[RAW_LOG_STREAM_SLOT_COUNT];

    uint32_t fill_slot_index;
    uint32_t drain_slot_index;
    uint32_t inflight_slot_indices[RAW_LOG_STREAM_DMA_BURST_MAX_BLOCKS];
    uint32_t inflight_slot_count;
    uint32_t ready_slot_count;
    uint32_t high_watermark_ready_slots;

    uint32_t produced_packet_count;
    uint32_t dropped_packet_count;
    uint32_t committed_block_count;

    uint32_t dummy_period_ms;
    uint32_t next_dummy_tick_ms;
    uint32_t dummy_seed;

    uint8_t inflight_valid;
} raw_log_stream_t;

void raw_log_stream_init_defaults(raw_log_stream_t *stream);
raw_log_writer_result_t raw_log_stream_begin(raw_log_stream_t *stream);

/*
 * Future sensor DMA path:
 *   1) acquire_write_ptr()
 *   2) sensor or DMA fills the returned region
 *   3) commit_write()
 * SD task drains READY slots independently.
 */
uint8_t *raw_log_stream_acquire_write_ptr(raw_log_stream_t *stream,
                                          uint32_t bytes,
                                          uint32_t *slot_index);

int raw_log_stream_commit_write(raw_log_stream_t *stream,
                                uint32_t slot_index,
                                uint32_t bytes,
                                uint32_t tick_ms);

void raw_log_stream_dummy_sensor_task(raw_log_stream_t *stream);

raw_log_writer_result_t raw_log_stream_sd_task(raw_log_stream_t *stream,
                                               raw_log_writer_step_info_t *step_info,
                                               uint8_t *step_ready);

#ifdef __cplusplus
}
#endif

#endif /* RAW_LOG_STREAM_H */
