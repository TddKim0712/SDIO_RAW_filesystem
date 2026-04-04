#include "sdrec_source_wave_table.h"
#include "sdrec_source_wave_table_data.h"

#include <string.h>

static void sdrec_wave_table_source_reset_ctx(void *ctx)
{
    sdrec_wave_table_source_reset((sdrec_wave_table_source_t *)ctx);
}

static sdrec_source_status_t sdrec_wave_table_source_poll_thunk(void *ctx,
                                                                sdrec_pipe_t *pipe)
{
    return sdrec_wave_table_source_poll((sdrec_wave_table_source_t *)ctx, pipe);
}

const sdrec_source_api_t g_sdrec_wave_table_source_api = {
    "table_cpu",
    SDREC_FLAG_PAYLOAD_TABLE_SENSOR,
    sdrec_wave_table_source_reset_ctx,
    sdrec_wave_table_source_poll_thunk,
};

void sdrec_wave_table_source_init_defaults(sdrec_wave_table_source_t *source)
{
    if (source == NULL)
    {
        return;
    }

    memset(source, 0, sizeof(*source));
    source->table = g_sdrec_source_sin360;
    source->table_sample_count = 360U;
    source->packet_samples = SDREC_WAVE_PACKET_MAX_SAMPLES;
    source->period_ms = SDREC_WAVE_TABLE_SOURCE_PERIOD_MS_DEFAULT;
}

void sdrec_wave_table_source_reset(sdrec_wave_table_source_t *source)
{
    if (source == NULL)
    {
        return;
    }

    source->source_packet_seq = 0U;
    source->next_table_index = 0U;
    source->next_deadline_ms = HAL_GetTick() + ((source->period_ms == 0U) ? 1U : source->period_ms);
}

void sdrec_wave_table_source_set_table(sdrec_wave_table_source_t *source,
                                    const int16_t *table,
                                    uint32_t table_sample_count)
{
    if ((source == NULL) || (table == NULL) || (table_sample_count == 0U))
    {
        return;
    }

    source->table = table;
    source->table_sample_count = table_sample_count;
}

void sdrec_wave_table_source_copy_samples(const sdrec_wave_table_source_t *source,
                                       int16_t *dst,
                                       uint32_t sample_count)
{
    uint32_t i;
    uint32_t index;

    if ((source == NULL) || (dst == NULL) || (source->table == NULL) ||
        (source->table_sample_count == 0U))
    {
        return;
    }

    for (i = 0U; i < sample_count; i++)
    {
        index = (source->next_table_index + i) % source->table_sample_count;
        dst[i] = source->table[index];
    }
}

void sdrec_wave_table_source_advance(sdrec_wave_table_source_t *source,
                                  uint32_t sample_count)
{
    if ((source == NULL) || (source->table_sample_count == 0U))
    {
        return;
    }

    source->next_table_index =
        (source->next_table_index + sample_count) % source->table_sample_count;
    source->source_packet_seq++;
}

void sdrec_wave_table_source_build_packet(sdrec_wave_table_source_t *source,
                                       sdrec_wave_packet_t *packet,
                                       uint32_t tick_ms)
{
    uint32_t samples_to_copy;

    if ((source == NULL) || (packet == NULL))
    {
        return;
    }

    memset(packet, 0, sizeof(*packet));
    packet->magic = SDREC_WAVE_PACKET_MAGIC;
    packet->source_packet_seq = source->source_packet_seq;
    packet->tick_ms = tick_ms;
    packet->table_index = source->next_table_index;
    samples_to_copy = source->packet_samples;
    if (samples_to_copy > SDREC_WAVE_PACKET_MAX_SAMPLES)
    {
        samples_to_copy = SDREC_WAVE_PACKET_MAX_SAMPLES;
    }
    packet->sample_count = samples_to_copy;
    packet->sample_format = SDREC_WAVE_PACKET_FMT_S16;

    sdrec_wave_table_source_copy_samples(source, packet->samples, samples_to_copy);
    sdrec_wave_table_source_advance(source, samples_to_copy);
}

sdrec_source_status_t sdrec_wave_table_source_poll(sdrec_wave_table_source_t *source,
                                                     sdrec_pipe_t *pipe)
{
    sdrec_wave_packet_t packet;
    uint32_t now;
    uint8_t produced_any = 0U;

    if ((source == NULL) || (pipe == NULL) || (source->period_ms == 0U) ||
        (source->table == NULL) || (source->table_sample_count == 0U))
    {
        return SDREC_SOURCE_ERR_PARAM;
    }

    now = HAL_GetTick();

    while ((int32_t)(now - source->next_deadline_ms) >= 0)
    {
        sdrec_wave_table_source_build_packet(source, &packet, now);
        (void)sdrec_pipe_push_packet(pipe,
                                         &packet,
                                         SDREC_PACKET_BYTES,
                                         now);
        source->next_deadline_ms += source->period_ms;
        produced_any = 1U;
        now = HAL_GetTick();
    }

    return (produced_any != 0U) ? SDREC_SOURCE_OK : SDREC_SOURCE_IDLE;
}
