#include "sdrec_source_api.h"
#include <stddef.h>

void sdrec_source_link_attach(sdrec_source_link_t *link,
                              const sdrec_source_api_t *api,
                              void *ctx)
{
    if (link == NULL)
    {
        return;
    }

    link->api = api;
    link->ctx = ctx;
}

void sdrec_source_link_detach(sdrec_source_link_t *link)
{
    if (link == NULL)
    {
        return;
    }

    link->api = NULL;
    link->ctx = NULL;
}

void sdrec_source_link_reset(sdrec_source_link_t *link)
{
    if ((link == NULL) || (link->api == NULL) || (link->api->reset == NULL))
    {
        return;
    }

    link->api->reset(link->ctx);
}

sdrec_source_status_t sdrec_source_link_poll(sdrec_source_link_t *link,
                                             sdrec_pipe_t *pipe)
{
    if ((link == NULL) || (link->api == NULL) || (link->api->poll == NULL))
    {
        return SDREC_SOURCE_IDLE;
    }

    return link->api->poll(link->ctx, pipe);
}
