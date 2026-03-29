#include "raw_crc32.h"

uint32_t raw_crc32_update(uint32_t crc, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    size_t i;
    uint32_t bit;

    if ((p == (const uint8_t *)0) || (len == 0U))
    {
        return crc;
    }

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint32_t)p[i];

        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

uint32_t raw_crc32_finalize(uint32_t crc)
{
    return (crc ^ 0xFFFFFFFFUL);
}

uint32_t raw_crc32_compute(const void *data, size_t len)
{
    return raw_crc32_finalize(raw_crc32_update(RAW_CRC32_INIT_VALUE, data, len));
}
