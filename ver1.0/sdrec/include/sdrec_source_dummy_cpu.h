#ifndef SDREC_SOURCE_DUMMY_CPU_H
#define SDREC_SOURCE_DUMMY_CPU_H

#include "sdrec_source_api.h"
#include "sdrec_layout_v3.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t period_ms;
    uint32_t next_deadline_ms;
    uint32_t seed;
    uint32_t source_packet_seq;
} sdrec_dummy_cpu_source_t;

void sdrec_dummy_cpu_source_init_defaults(sdrec_dummy_cpu_source_t *source);
void sdrec_dummy_cpu_source_reset(sdrec_dummy_cpu_source_t *source);
sdrec_source_status_t sdrec_dummy_cpu_source_poll(sdrec_dummy_cpu_source_t *source,
                                                     sdrec_pipe_t *pipe);

extern const sdrec_source_api_t g_sdrec_dummy_cpu_source_api;

#ifdef __cplusplus
}
#endif

#endif /* SDREC_SOURCE_DUMMY_CPU_H */
