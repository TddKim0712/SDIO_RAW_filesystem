#include "sdrec_source_wave_dma.h"

#include <string.h>

static void sdrec_wave_dma_source_reset_ctx(void *ctx)
{
    sdrec_wave_dma_source_reset((sdrec_wave_dma_source_t *)ctx);
}

static sdrec_source_status_t sdrec_wave_dma_source_poll_thunk(void *ctx,
                                                                    sdrec_pipe_t *pipe)
{
    return sdrec_wave_dma_source_poll((sdrec_wave_dma_source_t *)ctx, pipe);
}

const sdrec_source_api_t g_sdrec_wave_dma_source_api = {
    "table_dma_staged",
    SDREC_FLAG_PAYLOAD_TABLE_SENSOR | SDREC_FLAG_PAYLOAD_SENSOR_DMA,
    sdrec_wave_dma_source_reset_ctx,
    sdrec_wave_dma_source_poll_thunk,
};

void sdrec_wave_dma_source_init_defaults(sdrec_wave_dma_source_t *source)
{
    if (source == NULL)
    {
        return;
    }

    memset(source, 0, sizeof(*source));
    sdrec_wave_table_source_init_defaults(&source->table);
}

void sdrec_wave_dma_source_reset(sdrec_wave_dma_source_t *source)
{
    if (source == NULL)
    {
        return;
    }

    source->ready_mask = 0U;
    source->primed_mask = 0U;
    sdrec_wave_table_source_reset(&source->table);
}

sdrec_wave_packet_t *sdrec_wave_dma_source_get_buffer(sdrec_wave_dma_source_t *source,
                                                            uint32_t buffer_index)
{
    if ((source == NULL) || (buffer_index >= 2U))
    {
        return NULL;
    }

    return &source->staging[buffer_index];
}

void sdrec_wave_dma_source_refill(sdrec_wave_dma_source_t *source,
                                     uint32_t buffer_index,
                                     uint32_t tick_ms)
{
    if ((source == NULL) || (buffer_index >= 2U))
    {
        return;
    }

    sdrec_wave_table_source_build_packet(&source->table,
                                      &source->staging[buffer_index],
                                      tick_ms);
    source->primed_mask |= (uint8_t)(1U << buffer_index);
}

void sdrec_wave_dma_source_mark_ready(sdrec_wave_dma_source_t *source,
                                         uint32_t buffer_index)
{
    uint8_t bit;

    if ((source == NULL) || (buffer_index >= 2U))
    {
        return;
    }

    bit = (uint8_t)(1U << buffer_index);
    if ((source->primed_mask & bit) != 0U)
    {
        source->ready_mask |= bit;
    }
}

sdrec_source_status_t sdrec_wave_dma_source_poll(sdrec_wave_dma_source_t *source,
                                                         sdrec_pipe_t *pipe)
{
    uint32_t i;
    uint8_t bit;
    uint8_t produced_any = 0U;

    if ((source == NULL) || (pipe == NULL))
    {
        return SDREC_SOURCE_ERR_PARAM;
    }

    for (i = 0U; i < 2U; i++)
    {
        bit = (uint8_t)(1U << i);
        if ((source->ready_mask & bit) == 0U)
        {
            continue;
        }

        if (sdrec_pipe_push_packet(pipe,
                                       &source->staging[i],
                                       SDREC_PACKET_BYTES,
                                       source->staging[i].tick_ms) != 0)
        {
            source->ready_mask &= (uint8_t)~bit;
            source->primed_mask &= (uint8_t)~bit;
            produced_any = 1U;
        }
    }

    return (produced_any != 0U) ? SDREC_SOURCE_OK : SDREC_SOURCE_IDLE;
}
