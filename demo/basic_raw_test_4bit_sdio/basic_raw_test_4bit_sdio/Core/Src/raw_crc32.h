#ifndef RAW_CRC32_H
#define RAW_CRC32_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CRC-32/IEEE 802.3
 *   poly   = 0x04C11DB7
 *   refin  = true
 *   refout = true
 *   init   = 0xFFFFFFFF
 *   xorout = 0xFFFFFFFF
 */

#define RAW_CRC32_INIT_VALUE 0xFFFFFFFFUL

uint32_t raw_crc32_update(uint32_t crc, const void *data, size_t len);
uint32_t raw_crc32_finalize(uint32_t crc);
uint32_t raw_crc32_compute(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* RAW_CRC32_H */
