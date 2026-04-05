#ifndef SDREC_CRC32_H
#define SDREC_CRC32_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDREC_CRC32_INIT_VALUE 0xFFFFFFFFUL

uint32_t sdrec_crc32_update(uint32_t crc, const void *data, size_t len);
uint32_t sdrec_crc32_finalize(uint32_t crc);
uint32_t sdrec_crc32_compute(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* SDREC_CRC32_H */
