#include "sdrec_pipe.h"

#include <string.h>

static void sdrec_pipe_reset_slot(sdrec_pipe_slot_t *slot)
{
    if (slot == NULL)
    {
        return;
    }

    memset(slot, 0, sizeof(*slot));
    slot->state = SDREC_PIPE_SLOT_FREE;
}

static int sdrec_pipe_find_slot(const sdrec_pipe_t *pipe,
                                    uint32_t start_index,
                                    uint8_t wanted_state,
                                    uint32_t *slot_index)
{
    uint32_t i;
    uint32_t idx;

    if ((pipe == NULL) || (slot_index == NULL))
    {
        return 0;
    }

    for (i = 0U; i < SDREC_PIPE_SLOT_COUNT; i++)
    {
        idx = (start_index + i) % SDREC_PIPE_SLOT_COUNT;
        if (pipe->slots[idx].state == wanted_state)
        {
            *slot_index = idx;
            return 1;
        }
    }

    return 0;
}

static void sdrec_pipe_note_ready_slot(sdrec_pipe_t *pipe)
{
    if (pipe == NULL)
    {
        return;
    }

    pipe->queued_slot_count++;
    if (pipe->queued_slot_count > pipe->peak_queued_slot_count)
    {
        pipe->peak_queued_slot_count = pipe->queued_slot_count;
    }
}

static void sdrec_pipe_requeue_inflight(sdrec_pipe_t *pipe)
{
    sdrec_pipe_slot_t *slot;
    uint32_t i;
    uint32_t slot_index;

    if ((pipe == NULL) || (pipe->active_batch_valid == 0U))
    {
        return;
    }

    for (i = 0U; i < pipe->active_slot_count; i++)
    {
        slot_index = pipe->active_slot_idx_list[i];
        if (slot_index >= SDREC_PIPE_SLOT_COUNT)
        {
            continue;
        }

        slot = &pipe->slots[slot_index];
        if (slot->state == SDREC_PIPE_SLOT_INFLIGHT)
        {
            slot->state = SDREC_PIPE_SLOT_READY;
            sdrec_pipe_note_ready_slot(pipe);
        }
    }

    memset(pipe->active_slot_idx_list, 0, sizeof(pipe->active_slot_idx_list));
    pipe->active_slot_count = 0U;
    pipe->active_batch_valid = 0U;
}

static void sdrec_pipe_release_inflight(sdrec_pipe_t *pipe)
{
    uint32_t i;
    uint32_t slot_index;

    if ((pipe == NULL) || (pipe->active_batch_valid == 0U))
    {
        return;
    }

    for (i = 0U; i < pipe->active_slot_count; i++)
    {
        slot_index = pipe->active_slot_idx_list[i];
        if (slot_index >= SDREC_PIPE_SLOT_COUNT)
        {
            continue;
        }

        sdrec_pipe_reset_slot(&pipe->slots[slot_index]);
    }

    memset(pipe->active_slot_idx_list, 0, sizeof(pipe->active_slot_idx_list));
    pipe->active_slot_count = 0U;
    pipe->active_batch_valid = 0U;
}

static uint32_t sdrec_pipe_collect_ready_slots(sdrec_pipe_t *pipe,
                                                   const void **payloads,
                                                   uint32_t *payload_bytes,
                                                   uint32_t *last_slot_index)
{
    uint32_t i;
    uint32_t scan_index;
    uint32_t collected = 0U;
    uint32_t max_contiguous;

    if ((pipe == NULL) || (payloads == NULL) || (payload_bytes == NULL) ||
        (last_slot_index == NULL))
    {
        return 0U;
    }

    max_contiguous = sdrec_sink_get_max_contiguous_write_blocks(&pipe->sink);
    if (max_contiguous == 0U)
    {
        return 0U;
    }

    if (max_contiguous > SDREC_PIPE_MAX_BATCH_BLOCKS)
    {
        max_contiguous = SDREC_PIPE_MAX_BATCH_BLOCKS;
    }

    for (i = 0U; (i < SDREC_PIPE_SLOT_COUNT) && (collected < max_contiguous); i++)
    {
        scan_index = (pipe->egress_slot_idx + i) % SDREC_PIPE_SLOT_COUNT;
        if (pipe->slots[scan_index].state != SDREC_PIPE_SLOT_READY)
        {
            continue;
        }

        pipe->slots[scan_index].state = SDREC_PIPE_SLOT_INFLIGHT;
        pipe->active_slot_idx_list[collected] = scan_index;
        payloads[collected] = pipe->slots[scan_index].payload.bytes;
        payload_bytes[collected] = pipe->slots[scan_index].payload_bytes;
        *last_slot_index = scan_index;
        collected++;
    }

    return collected;
}

void sdrec_pipe_init_defaults(sdrec_pipe_t *pipe)
{
    uint32_t i;

    if (pipe == NULL)
    {
        return;
    }

    memset(pipe, 0, sizeof(*pipe));
    sdrec_sink_init_defaults(&pipe->sink);

    for (i = 0U; i < SDREC_PIPE_SLOT_COUNT; i++)
    {
        sdrec_pipe_reset_slot(&pipe->slots[i]);
    }

    pipe->ingress_slot_idx = 0U;
    pipe->egress_slot_idx = 0U;
}

void sdrec_pipe_reset_stats(sdrec_pipe_t *pipe)
{
    if (pipe == NULL)
    {
        return;
    }

    pipe->queued_slot_count = 0U;
    pipe->peak_queued_slot_count = 0U;
    pipe->ingress_packet_count = 0U;
    pipe->dropped_packet_count = 0U;
    pipe->written_block_count = 0U;
}

void sdrec_pipe_note_source_drop(sdrec_pipe_t *pipe)
{
    if (pipe == NULL)
    {
        return;
    }

    pipe->dropped_packet_count++;
}

sdrec_sink_status_t sdrec_pipe_open(sdrec_pipe_t *pipe)
{
    uint32_t i;

    if (pipe == NULL)
    {
        return SDREC_SINK_ERR_PARAM;
    }

    sdrec_card_reset_retry_stats();

    for (i = 0U; i < SDREC_PIPE_SLOT_COUNT; i++)
    {
        sdrec_pipe_reset_slot(&pipe->slots[i]);
    }

    pipe->ingress_slot_idx = 0U;
    pipe->egress_slot_idx = 0U;
    memset(pipe->active_slot_idx_list, 0, sizeof(pipe->active_slot_idx_list));
    pipe->active_slot_count = 0U;
    sdrec_pipe_reset_stats(pipe);
    pipe->active_batch_valid = 0U;

    return sdrec_sink_open(&pipe->sink);
}

uint8_t *sdrec_pipe_acquire_ingress_ptr(sdrec_pipe_t *pipe,
                                          uint32_t bytes,
                                          uint32_t *slot_index)
{
    sdrec_pipe_slot_t *slot;
    uint32_t next_slot_index;

    if ((pipe == NULL) || (bytes == 0U) || (bytes > SDREC_DATA_PAYLOAD_BYTES_V3))
    {
        return NULL;
    }

    slot = &pipe->slots[pipe->ingress_slot_idx];

    if (slot->state == SDREC_PIPE_SLOT_FREE)
    {
        slot->state = SDREC_PIPE_SLOT_FILLING;
        slot->payload_bytes = 0U;
    }

    if ((slot->state != SDREC_PIPE_SLOT_FILLING) ||
        ((slot->payload_bytes + bytes) > SDREC_DATA_PAYLOAD_BYTES_V3))
    {
        if (!sdrec_pipe_find_slot(pipe,
                                      (pipe->ingress_slot_idx + 1U),
                                      SDREC_PIPE_SLOT_FREE,
                                      &next_slot_index))
        {
            return NULL;
        }

        pipe->ingress_slot_idx = next_slot_index;
        slot = &pipe->slots[pipe->ingress_slot_idx];
        memset(slot, 0, sizeof(*slot));
        slot->state = SDREC_PIPE_SLOT_FILLING;
    }

    if (slot_index != NULL)
    {
        *slot_index = pipe->ingress_slot_idx;
    }

    return &slot->payload.bytes[slot->payload_bytes];
}

int sdrec_pipe_commit_ingress(sdrec_pipe_t *pipe,
                                uint32_t slot_index,
                                uint32_t bytes,
                                uint32_t tick_ms)
{
    sdrec_pipe_slot_t *slot;
    uint32_t next_slot_index;

    if ((pipe == NULL) || (slot_index >= SDREC_PIPE_SLOT_COUNT) || (bytes == 0U))
    {
        return 0;
    }

    slot = &pipe->slots[slot_index];
    if (slot->state != SDREC_PIPE_SLOT_FILLING)
    {
        return 0;
    }

    if ((slot->payload_bytes + bytes) > SDREC_DATA_PAYLOAD_BYTES_V3)
    {
        return 0;
    }

    slot->payload_bytes += bytes;
    slot->packet_tick_ms = tick_ms;
    pipe->ingress_packet_count++;

    if (slot->payload_bytes == SDREC_DATA_PAYLOAD_BYTES_V3)
    {
        slot->state = SDREC_PIPE_SLOT_READY;
        sdrec_pipe_note_ready_slot(pipe);

        if (sdrec_pipe_find_slot(pipe,
                                     (slot_index + 1U),
                                     SDREC_PIPE_SLOT_FREE,
                                     &next_slot_index))
        {
            pipe->ingress_slot_idx = next_slot_index;
        }
    }

    return 1;
}

int sdrec_pipe_push_packet(sdrec_pipe_t *pipe,
                               const void *packet,
                               uint32_t bytes,
                               uint32_t tick_ms)
{
    uint32_t slot_index;
    uint8_t *dst;

    if ((pipe == NULL) || ((packet == NULL) && (bytes != 0U)))
    {
        return 0;
    }

    dst = sdrec_pipe_acquire_ingress_ptr(pipe, bytes, &slot_index);
    if (dst == NULL)
    {
        sdrec_pipe_note_source_drop(pipe);
        return 0;
    }

    memcpy(dst, packet, bytes);

    if (sdrec_pipe_commit_ingress(pipe, slot_index, bytes, tick_ms) == 0)
    {
        sdrec_pipe_note_source_drop(pipe);
        return 0;
    }

    return 1;
}

sdrec_sink_status_t sdrec_pipe_drain_to_card(sdrec_pipe_t *pipe,
                                               sdrec_commit_report_t *step_info,
                                               uint8_t *step_ready)
{
    sdrec_sink_status_t writer_result;
    const void *payloads[SDREC_PIPE_MAX_BATCH_BLOCKS];
    uint32_t payload_sizes[SDREC_PIPE_MAX_BATCH_BLOCKS];
    uint32_t ready_count;
    uint32_t last_slot_index = 0U;
    uint32_t committed_blocks;

    if (step_ready != NULL)
    {
        *step_ready = 0U;
    }

    if (pipe == NULL)
    {
        return SDREC_SINK_ERR_PARAM;
    }

    if (pipe->active_batch_valid != 0U)
    {
        committed_blocks = pipe->active_slot_count;

        writer_result = sdrec_sink_poll_async(&pipe->sink, step_info);
        if (writer_result == SDREC_SINK_IN_PROGRESS)
        {
            return SDREC_SINK_IN_PROGRESS;
        }

        if (writer_result != SDREC_SINK_OK)
        {
            sdrec_pipe_requeue_inflight(pipe);
            return writer_result;
        }

        sdrec_pipe_release_inflight(pipe);
        pipe->written_block_count += committed_blocks;

        if (step_ready != NULL)
        {
            *step_ready = 1U;
        }

        return SDREC_SINK_OK;
    }

    if (pipe->queued_slot_count == 0U)
    {
        return SDREC_SINK_OK;
    }

    memset(payloads, 0, sizeof(payloads));
    memset(payload_sizes, 0, sizeof(payload_sizes));
    memset(pipe->active_slot_idx_list, 0, sizeof(pipe->active_slot_idx_list));

    ready_count = sdrec_pipe_collect_ready_slots(pipe,
                                                     payloads,
                                                     payload_sizes,
                                                     &last_slot_index);
    if (ready_count == 0U)
    {
        return SDREC_SINK_OK;
    }

    pipe->queued_slot_count -= ready_count;
    pipe->active_slot_count = ready_count;
    pipe->active_batch_valid = 1U;
    pipe->egress_slot_idx = (last_slot_index + 1U) % SDREC_PIPE_SLOT_COUNT;

    writer_result = sdrec_sink_begin_async_batch(&pipe->sink,
                                                        payloads,
                                                        payload_sizes,
                                                        ready_count);
    if (writer_result != SDREC_SINK_OK)
    {
        sdrec_pipe_requeue_inflight(pipe);
        return writer_result;
    }

    return SDREC_SINK_IN_PROGRESS;
}
