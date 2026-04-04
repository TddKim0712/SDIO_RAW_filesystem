#ifndef SDREC_SOURCE_WAVE_DMA_H
#define SDREC_SOURCE_WAVE_DMA_H

#include "sdrec_source_wave_table.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    sdrec_wave_table_source_t table;
    sdrec_wave_packet_t staging[2];
    uint8_t ready_mask;
    uint8_t primed_mask;
} sdrec_wave_dma_source_t;

void sdrec_wave_dma_source_init_defaults(sdrec_wave_dma_source_t *source);
void sdrec_wave_dma_source_reset(sdrec_wave_dma_source_t *source);

sdrec_wave_packet_t *sdrec_wave_dma_source_get_buffer(sdrec_wave_dma_source_t *source,
                                                            uint32_t buffer_index);

void sdrec_wave_dma_source_refill(sdrec_wave_dma_source_t *source,
                                     uint32_t buffer_index,
                                     uint32_t tick_ms);

void sdrec_wave_dma_source_mark_ready(sdrec_wave_dma_source_t *source,
                                         uint32_t buffer_index);

sdrec_source_status_t sdrec_wave_dma_source_poll(sdrec_wave_dma_source_t *source,
                                                         sdrec_pipe_t *pipe);

extern const sdrec_source_api_t g_sdrec_wave_dma_source_api;

#ifdef __cplusplus
}
#endif

#endif /* SDREC_SOURCE_WAVE_DMA_H */
