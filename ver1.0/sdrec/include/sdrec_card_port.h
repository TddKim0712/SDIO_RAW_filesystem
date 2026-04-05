#ifndef SDREC_CARD_PORT_H
#define SDREC_CARD_PORT_H

#include "sdrec_build_config.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SDIO_CK = SDIOCLK / (ClockDiv + 2)
 * With SDIOCLK = 48 MHz:
 *   init div 118 -> 400 kHz
 *   xfer div   0 -> 24 MHz
 *   xfer div   2 -> 12 MHz
 *   xfer div  46 -> 1 MHz
 */
#define SDREC_CARD_INIT_CLOCK_DIV_DEFAULT     118U
#define SDREC_CARD_TIMEOUT_READY_MS          1000U
#define SDREC_CARD_TIMEOUT_READ_BASE_MS      1000U
#define SDREC_CARD_TIMEOUT_WRITE_BASE_MS     2000U
#define SDREC_CARD_TIMEOUT_PER_BLOCK_MS       100U

typedef enum
{
    SDREC_CARD_OK = 0,
    SDREC_CARD_IN_PROGRESS = 1,
    SDREC_CARD_ERR_PARAM = -1,
    SDREC_CARD_ERR_ALIGN = -2,
    SDREC_CARD_ERR_INIT = -3,
    SDREC_CARD_ERR_WIDEBUS = -4,
    SDREC_CARD_ERR_READ = -5,
    SDREC_CARD_ERR_WRITE = -6,
    SDREC_CARD_ERR_TIMEOUT = -7,
    SDREC_CARD_ERR_STATE = -8
} sdrec_card_status_t;

typedef enum
{
    SDREC_CARD_BUS_1BIT = 0,
    SDREC_CARD_BUS_4BIT = 1
} sdrec_card_bus_width_t;

typedef struct
{
    uint32_t read_retry_count;
    uint32_t write_retry_count;
    uint32_t recover_count;
    uint32_t async_restart_count;
    sdrec_card_status_t last_status;
    uint32_t last_hal_error;
} sdrec_card_retry_stats_t;

typedef struct
{
    uint32_t read_retry_count;
    uint32_t write_retry_count;
    uint32_t recovery_delay_ms;
} sdrec_card_policy_t;

void sdrec_card_policy_init_defaults(sdrec_card_policy_t *policy);
void sdrec_card_set_policy(const sdrec_card_policy_t *policy);
void sdrec_card_get_policy(sdrec_card_policy_t *policy);

uint32_t sdrec_card_get_last_error(void);
HAL_SD_CardStateTypeDef sdrec_card_get_card_state(void);
void sdrec_card_get_card_info(HAL_SD_CardInfoTypeDef *info);
void sdrec_card_get_retry_stats(sdrec_card_retry_stats_t *stats);
void sdrec_card_reset_retry_stats(void);

sdrec_card_status_t sdrec_card_wait_ready(uint32_t timeout_ms);

sdrec_card_status_t sdrec_card_init(sdrec_card_bus_width_t bus_width,
                            uint32_t transfer_clock_div);

sdrec_card_status_t sdrec_card_reconfigure(sdrec_card_bus_width_t bus_width,
                                   uint32_t transfer_clock_div);

sdrec_card_status_t sdrec_card_read_blocks(uint32_t start_lba,
                                   void *buf,
                                   uint32_t block_count);

sdrec_card_status_t sdrec_card_write_blocks(uint32_t start_lba,
                                    const void *buf,
                                    uint32_t block_count);

sdrec_card_status_t sdrec_card_write_async_begin(uint32_t start_lba,
                                                const void *buf,
                                                uint32_t block_count);

sdrec_card_status_t sdrec_card_write_async_poll(void);

int sdrec_card_write_async_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* SDREC_CARD_PORT_H */
