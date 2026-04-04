#include "sdrec_runtime.h"
#include <stddef.h>

void sdrec_runtime_cfg_init_defaults(sdrec_runtime_cfg_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    cfg->bus_width = (sdrec_card_bus_width_t)SDREC_RUNTIME_BUS_WIDTH_DEFAULT;
    cfg->transfer_clock_div = SDREC_RUNTIME_CLOCK_DIV_DEFAULT;
    cfg->log_every_n_blocks = SDREC_RUNTIME_LOG_EVERY_N_BLOCKS_DEFAULT;
    cfg->payload_flags = 0U;
    sdrec_card_policy_init_defaults(&cfg->sd_policy);
    sdrec_layout_cfg_init_defaults(&cfg->layout_cfg);
}

void sdrec_runtime_init_defaults(sdrec_runtime_t *runtime)
{
    if (runtime == NULL)
    {
        return;
    }

    sdrec_pipe_init_defaults(&runtime->pipe);
    sdrec_source_link_detach(&runtime->source_link);
    sdrec_runtime_cfg_init_defaults(&runtime->cfg);
}

void sdrec_runtime_attach_source(sdrec_runtime_t *runtime,
                                 const sdrec_source_api_t *api,
                                 void *ctx)
{
    if (runtime == NULL)
    {
        return;
    }

    sdrec_source_link_attach(&runtime->source_link, api, ctx);

    if ((runtime->cfg.payload_flags == 0U) && (api != NULL))
    {
        runtime->cfg.payload_flags = api->default_payload_flags;
    }
}

void sdrec_runtime_detach_source(sdrec_runtime_t *runtime)
{
    if (runtime == NULL)
    {
        return;
    }

    sdrec_source_link_detach(&runtime->source_link);
}

sdrec_sink_status_t sdrec_runtime_start(sdrec_runtime_t *runtime)
{
    if (runtime == NULL)
    {
        return SDREC_SINK_ERR_PARAM;
    }

    sdrec_card_set_policy(&runtime->cfg.sd_policy);

    if (sdrec_card_init(runtime->cfg.bus_width,
                        runtime->cfg.transfer_clock_div) != SDREC_CARD_OK)
    {
        return SDREC_SINK_ERR_CARD_WRITE;
    }

    sdrec_sink_set_runtime_config(&runtime->pipe.sink,
                                  &runtime->cfg.layout_cfg,
                                  runtime->cfg.log_every_n_blocks,
                                  (runtime->cfg.payload_flags != 0U) ?
                                      runtime->cfg.payload_flags :
                                      runtime->pipe.sink.payload_flags);

    sdrec_source_link_reset(&runtime->source_link);
    return sdrec_pipe_open(&runtime->pipe);
}

sdrec_sink_status_t sdrec_runtime_poll(sdrec_runtime_t *runtime,
                                       sdrec_commit_report_t *report,
                                       uint8_t *report_ready)
{
    sdrec_source_status_t source_status;

    if (runtime == NULL)
    {
        return SDREC_SINK_ERR_PARAM;
    }

    source_status = sdrec_source_link_poll(&runtime->source_link, &runtime->pipe);
    if (source_status < 0)
    {
        return SDREC_SINK_ERR_STATE;
    }

    return sdrec_pipe_drain_to_card(&runtime->pipe, report, report_ready);
}
