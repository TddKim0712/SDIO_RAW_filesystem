#ifndef SDREC_SOURCE_API_H
#define SDREC_SOURCE_API_H

#include "sdrec_pipe.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SDREC_SOURCE_OK = 0,
    SDREC_SOURCE_IDLE = 1,
    SDREC_SOURCE_ERR_PARAM = -1,
    SDREC_SOURCE_ERR_PIPE = -2
} sdrec_source_status_t;

typedef struct
{
    const char *name;
    uint32_t default_payload_flags;
    void (*reset)(void *ctx);
    sdrec_source_status_t (*poll)(void *ctx, sdrec_pipe_t *pipe);
} sdrec_source_api_t;

typedef struct
{
    const sdrec_source_api_t *api;
    void *ctx;
} sdrec_source_link_t;

void sdrec_source_link_attach(sdrec_source_link_t *link,
                              const sdrec_source_api_t *api,
                              void *ctx);

void sdrec_source_link_detach(sdrec_source_link_t *link);

void sdrec_source_link_reset(sdrec_source_link_t *link);

sdrec_source_status_t sdrec_source_link_poll(sdrec_source_link_t *link,
                                             sdrec_pipe_t *pipe);

#ifdef __cplusplus
}
#endif

#endif /* SDREC_SOURCE_API_H */
