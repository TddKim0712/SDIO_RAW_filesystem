#ifndef RAW_DISKIO_H
#define RAW_DISKIO_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RAW_SD_BLOCK_SIZE
#define RAW_SD_BLOCK_SIZE 512U
#endif

/*
 * SDIO_CK = SDIOCLK / (ClockDiv + 2)
 * With SDIOCLK = 48 MHz:
 *   init div 118 -> 400 kHz
 *   xfer div   0 -> 24 MHz
 *   xfer div   2 -> 12 MHz
 *   xfer div  46 -> 1 MHz
 */
#define RAW_SD_INIT_CLOCK_DIV_DEFAULT     118U
#define RAW_SD_TIMEOUT_READY_MS          1000U
#define RAW_SD_TIMEOUT_READ_BASE_MS      1000U
#define RAW_SD_TIMEOUT_WRITE_BASE_MS     2000U
#define RAW_SD_TIMEOUT_PER_BLOCK_MS       100U

typedef enum
{
    RAW_SD_OK = 0,
    RAW_SD_IN_PROGRESS = 1,
    RAW_SD_ERR_PARAM = -1,
    RAW_SD_ERR_ALIGN = -2,
    RAW_SD_ERR_INIT = -3,
    RAW_SD_ERR_WIDEBUS = -4,
    RAW_SD_ERR_READ = -5,
    RAW_SD_ERR_WRITE = -6,
    RAW_SD_ERR_TIMEOUT = -7,
    RAW_SD_ERR_STATE = -8
} raw_sd_status_t;

typedef enum
{
    RAW_SD_BUS_1BIT = 0,
    RAW_SD_BUS_4BIT = 1
} raw_sd_bus_width_t;

typedef struct
{
    uint32_t read_retry_count;
    uint32_t write_retry_count;
    uint32_t recover_count;
    uint32_t async_restart_count;
    raw_sd_status_t last_status;
    uint32_t last_hal_error;
} raw_sd_retry_stats_t;

uint32_t raw_sd_get_last_error(void);
HAL_SD_CardStateTypeDef raw_sd_get_card_state(void);
void raw_sd_get_card_info(HAL_SD_CardInfoTypeDef *info);
void raw_sd_get_retry_stats(raw_sd_retry_stats_t *stats);
void raw_sd_reset_retry_stats(void);

raw_sd_status_t raw_sd_wait_ready(uint32_t timeout_ms);

/*
 * Initializes the card in 1-bit mode at <= 400 kHz, then switches to the
 * requested transfer bus width and transfer clock divider.
 */
raw_sd_status_t raw_sd_init(raw_sd_bus_width_t bus_width,
                            uint32_t transfer_clock_div);

/*
 * Re-applies bus width / transfer clock after init.
 * Useful when you want to start in 1-bit and later try 4-bit.
 */
raw_sd_status_t raw_sd_reconfigure(raw_sd_bus_width_t bus_width,
                                   uint32_t transfer_clock_div);

raw_sd_status_t raw_sd_read_blocks(uint32_t start_lba,
                                   void *buf,
                                   uint32_t block_count);

raw_sd_status_t raw_sd_write_blocks(uint32_t start_lba,
                                    const void *buf,
                                    uint32_t block_count);

/*
 * Non-blocking SDIO DMA write path.
 * Start once, then keep calling service() until it returns RAW_SD_OK or an error.
 */
raw_sd_status_t raw_sd_write_blocks_async_start(uint32_t start_lba,
                                                const void *buf,
                                                uint32_t block_count);

raw_sd_status_t raw_sd_write_blocks_async_service(void);

int raw_sd_write_blocks_async_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* RAW_DISKIO_H */
