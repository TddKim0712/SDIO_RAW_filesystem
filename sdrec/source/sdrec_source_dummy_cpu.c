#include "sdrec_source_dummy_cpu.h"

#include <string.h>

static void sdrec_dummy_cpu_source_fill_packet(sdrec_dummy_cpu_source_t *source,
                                             sdrec_pipe_t *pipe,
                                             sdrec_dummy_cpu_packet_t *packet,
                                             uint32_t now)
{
    uint32_t i;

    memset(packet, 0, sizeof(*packet));
    packet->magic = SDREC_DUMMY_CPU_PACKET_MAGIC;
    packet->source_packet_seq = source->source_packet_seq;
    packet->tick_ms = now;
    packet->sink_seq_snapshot = pipe->sink.layout_state.write_seq;
    packet->ready_slots_snapshot = pipe->queued_slot_count;
    packet->dropped_packets_snapshot = pipe->dropped_packet_count;
    packet->committed_blocks_snapshot = pipe->written_block_count;
    packet->reserved0 = source->seed;

    for (i = 0U; i < sizeof(packet->tail); i++)
    {
        packet->tail[i] = (uint8_t)((source->seed + now + i) & 0xFFU);
    }
}

static void sdrec_dummy_cpu_source_reset_ctx(void *ctx)
{
    sdrec_dummy_cpu_source_reset((sdrec_dummy_cpu_source_t *)ctx);
}

static sdrec_source_status_t sdrec_dummy_cpu_source_poll_thunk(void *ctx,
                                                                sdrec_pipe_t *pipe)
{
    return sdrec_dummy_cpu_source_poll((sdrec_dummy_cpu_source_t *)ctx, pipe);
}

const sdrec_source_api_t g_sdrec_dummy_cpu_source_api = {
    "dummy_cpu",
    SDREC_FLAG_PAYLOAD_DUMMY_SENSOR,
    sdrec_dummy_cpu_source_reset_ctx,
    sdrec_dummy_cpu_source_poll_thunk,
};

void sdrec_dummy_cpu_source_init_defaults(sdrec_dummy_cpu_source_t *source)
{
    if (source == NULL)
    {
        return;
    }

    memset(source, 0, sizeof(*source));
    source->period_ms = SDREC_DUMMY_CPU_SOURCE_PERIOD_MS_DEFAULT;
    source->seed = 0x12340000UL;
}

void sdrec_dummy_cpu_source_reset(sdrec_dummy_cpu_source_t *source)
{
    if (source == NULL)
    {
        return;
    }

    source->source_packet_seq = 0U;
    source->next_deadline_ms = HAL_GetTick() + ((source->period_ms == 0U) ? 1U : source->period_ms);
}

sdrec_source_status_t sdrec_dummy_cpu_source_poll(sdrec_dummy_cpu_source_t *source,
                                                     sdrec_pipe_t *pipe)
{
    sdrec_dummy_cpu_packet_t packet;
    uint32_t now;
    uint8_t produced_any = 0U;

    if ((source == NULL) || (pipe == NULL) || (source->period_ms == 0U))
    {
        return SDREC_SOURCE_ERR_PARAM;
    }

    now = HAL_GetTick();

    while ((int32_t)(now - source->next_deadline_ms) >= 0)
    {
        sdrec_dummy_cpu_source_fill_packet(source, pipe, &packet, now);
        (void)sdrec_pipe_push_packet(pipe,
                                         &packet,
                                         SDREC_PACKET_BYTES,
                                         now);
        source->source_packet_seq++;
        source->seed += 0x31U;
        source->next_deadline_ms += source->period_ms;
        produced_any = 1U;
        now = HAL_GetTick();
    }

    return (produced_any != 0U) ? SDREC_SOURCE_OK : SDREC_SOURCE_IDLE;
}
