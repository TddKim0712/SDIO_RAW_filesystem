#ifndef SDREC_RUNTIME_H
#define SDREC_RUNTIME_H

#include "sdrec_source_api.h"
#include "sdrec_card_port.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    sdrec_card_bus_width_t bus_width;
    uint32_t transfer_clock_div;
    uint32_t log_every_n_blocks;
    uint32_t payload_flags;
    sdrec_card_policy_t sd_policy;
    sdrec_layout_cfg_t layout_cfg;
} sdrec_runtime_cfg_t;

typedef struct
{
    sdrec_pipe_t pipe;
    sdrec_source_link_t source_link;
    sdrec_runtime_cfg_t cfg;
} sdrec_runtime_t;

void sdrec_runtime_cfg_init_defaults(sdrec_runtime_cfg_t *cfg);
void sdrec_runtime_init_defaults(sdrec_runtime_t *runtime);

void sdrec_runtime_attach_source(sdrec_runtime_t *runtime,
                                 const sdrec_source_api_t *api,
                                 void *ctx);

void sdrec_runtime_detach_source(sdrec_runtime_t *runtime);

sdrec_sink_status_t sdrec_runtime_start(sdrec_runtime_t *runtime);

sdrec_sink_status_t sdrec_runtime_poll(sdrec_runtime_t *runtime,
                                       sdrec_commit_report_t *report,
                                       uint8_t *report_ready);

#ifdef __cplusplus
}
#endif

#endif /* SDREC_RUNTIME_H */
