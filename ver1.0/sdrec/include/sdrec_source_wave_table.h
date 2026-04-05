#ifndef SDREC_SOURCE_WAVE_TABLE_H
#define SDREC_SOURCE_WAVE_TABLE_H

#include "sdrec_source_api.h"
#include "sdrec_layout_v3.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const int16_t *table;
    uint32_t table_sample_count;
    uint32_t packet_samples;
    uint32_t period_ms;
    uint32_t next_deadline_ms;
    uint32_t source_packet_seq;
    uint32_t next_table_index;
} sdrec_wave_table_source_t;

void sdrec_wave_table_source_init_defaults(sdrec_wave_table_source_t *source);
void sdrec_wave_table_source_reset(sdrec_wave_table_source_t *source);
void sdrec_wave_table_source_set_table(sdrec_wave_table_source_t *source,
                                    const int16_t *table,
                                    uint32_t table_sample_count);

void sdrec_wave_table_source_build_packet(sdrec_wave_table_source_t *source,
                                       sdrec_wave_packet_t *packet,
                                       uint32_t tick_ms);

void sdrec_wave_table_source_copy_samples(const sdrec_wave_table_source_t *source,
                                       int16_t *dst,
                                       uint32_t sample_count);

void sdrec_wave_table_source_advance(sdrec_wave_table_source_t *source,
                                  uint32_t sample_count);

sdrec_source_status_t sdrec_wave_table_source_poll(sdrec_wave_table_source_t *source,
                                                     sdrec_pipe_t *pipe);

extern const sdrec_source_api_t g_sdrec_wave_table_source_api;

#ifdef __cplusplus
}
#endif

#endif /* SDREC_SOURCE_WAVE_TABLE_H */
