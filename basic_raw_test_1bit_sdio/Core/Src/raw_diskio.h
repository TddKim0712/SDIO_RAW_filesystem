#ifndef RAW_DISKIO_H
#define RAW_DISKIO_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAW_SD_BLOCK_SIZE 512U

typedef enum
{
    RAW_SD_OK = 0,
    RAW_SD_ERR_PARAM = -1,
    RAW_SD_ERR_INIT = -2,
    RAW_SD_ERR_READ = -3,
    RAW_SD_ERR_WRITE = -4,
    RAW_SD_ERR_TIMEOUT = -5,
    RAW_SD_ERR_STATE = -6,
    RAW_SD_ERR_ALIGN = -7,
} raw_sd_status_t;

raw_sd_status_t raw_sd_init(void);
raw_sd_status_t raw_sd_wait_ready(uint32_t timeout_ms);
raw_sd_status_t raw_sd_read_blocks(uint32_t start_lba, void *buf, uint32_t block_count);
raw_sd_status_t raw_sd_write_blocks(uint32_t start_lba, const void *buf, uint32_t block_count);
raw_sd_status_t raw_sd_fill_blocks(uint32_t start_lba, uint32_t block_count, uint8_t value);

HAL_SD_CardStateTypeDef raw_sd_get_card_state(void);
void raw_sd_get_card_info(HAL_SD_CardInfoTypeDef *info);
uint32_t raw_sd_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* RAW_DISKIO_H */
