#ifndef SDREC_PIPE_H
#define SDREC_PIPE_H

#include "sdrec_sink.h"
#include "sdrec_build_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SDREC_PIPE_MAX_BATCH_BLOCKS
#if (SDREC_PIPE_SLOT_COUNT < SDREC_SINK_MAX_ASYNC_BLOCKS)
#define SDREC_PIPE_MAX_BATCH_BLOCKS SDREC_PIPE_SLOT_COUNT
#else
#define SDREC_PIPE_MAX_BATCH_BLOCKS SDREC_SINK_MAX_ASYNC_BLOCKS
#endif
#endif

typedef enum
{
    SDREC_PIPE_SLOT_FREE = 0,
    SDREC_PIPE_SLOT_FILLING = 1,
    SDREC_PIPE_SLOT_READY = 2,
    SDREC_PIPE_SLOT_INFLIGHT = 3
} sdrec_pipe_slot_state_t;

typedef union
{
    uint32_t words[(SDREC_DATA_PAYLOAD_BYTES_V3 + sizeof(uint32_t) - 1U) / sizeof(uint32_t)];
    uint8_t  bytes[SDREC_DATA_PAYLOAD_BYTES_V3];
} sdrec_pipe_slot_payload_t;

typedef struct
{
    sdrec_pipe_slot_payload_t payload;
    uint32_t payload_bytes;
    uint32_t packet_tick_ms;
    uint8_t  state;
    uint8_t  reserved[3];
} sdrec_pipe_slot_t;

typedef struct
{
    sdrec_sink_t sink;
    sdrec_pipe_slot_t slots[SDREC_PIPE_SLOT_COUNT];

    uint32_t ingress_slot_idx;
    uint32_t egress_slot_idx;
    uint32_t active_slot_idx_list[SDREC_PIPE_MAX_BATCH_BLOCKS];
    uint32_t active_slot_count;
    uint32_t queued_slot_count;
    uint32_t peak_queued_slot_count;

    uint32_t ingress_packet_count;
    uint32_t dropped_packet_count;
    uint32_t written_block_count;

    uint8_t active_batch_valid;
} sdrec_pipe_t;

void sdrec_pipe_init_defaults(sdrec_pipe_t *pipe);
sdrec_sink_status_t sdrec_pipe_open(sdrec_pipe_t *pipe);
void sdrec_pipe_reset_stats(sdrec_pipe_t *pipe);
void sdrec_pipe_note_source_drop(sdrec_pipe_t *pipe);

/*
 * Generic ingress API.
 * - CPU producer can call sdrec_pipe_push_packet().
 * - DMA-capable producer can call sdrec_pipe_acquire_ingress_ptr()
 *   and sdrec_pipe_commit_ingress().
 * The pipe itself does not know anything about how packets are produced.
 */
uint8_t *sdrec_pipe_acquire_ingress_ptr(sdrec_pipe_t *pipe,
                                        uint32_t bytes,
                                        uint32_t *slot_index);

int sdrec_pipe_commit_ingress(sdrec_pipe_t *pipe,
                              uint32_t slot_index,
                              uint32_t bytes,
                              uint32_t tick_ms);

int sdrec_pipe_push_packet(sdrec_pipe_t *pipe,
                           const void *packet,
                           uint32_t bytes,
                           uint32_t tick_ms);

sdrec_sink_status_t sdrec_pipe_drain_to_card(sdrec_pipe_t *pipe,
                                             sdrec_commit_report_t *report,
                                             uint8_t *report_ready);

#ifdef __cplusplus
}
#endif

#endif /* SDREC_PIPE_H */
