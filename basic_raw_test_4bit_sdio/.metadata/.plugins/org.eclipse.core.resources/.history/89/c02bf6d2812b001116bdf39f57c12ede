#ifndef RAW_LOG_CORE_H
#define RAW_LOG_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RAW_SD_BLOCK_SIZE
#define RAW_SD_BLOCK_SIZE 512U
#endif

#define RAW_LOG_LAYOUT_VERSION_V3                 3U
#define RAW_LOG_SUPER_MAGIC_V3                    "RAWSDIO3"
#define RAW_LOG_DATA_MAGIC_V3                     "RAWDATA3"
#define RAW_LOG_TAIL_MAGIC_V3                     0xA55A5AA5UL

/*
 * Future DMA/ping-pong planning constants.
 * The current writer still uses polling SD writes, but these constants keep the
 * staging math ready for a later sensor DMA path.
 */
#define RAW_LOG_SENSOR_PACKET_BYTES               64U
#define RAW_LOG_DATA_HEADER_BYTES_V3              64U
#define RAW_LOG_DATA_PAYLOAD_BYTES_V3             (RAW_SD_BLOCK_SIZE - RAW_LOG_DATA_HEADER_BYTES_V3)
#define RAW_LOG_PACKETS_PER_DATA_BLOCK_V3         (RAW_LOG_DATA_PAYLOAD_BYTES_V3 / RAW_LOG_SENSOR_PACKET_BYTES)
#define RAW_LOG_RECOMMENDED_BURST_BLOCKS          8U
#define RAW_LOG_RECOMMENDED_BURST_BYTES           (RAW_LOG_RECOMMENDED_BURST_BLOCKS * RAW_SD_BLOCK_SIZE)
#define RAW_LOG_RECOMMENDED_BURST_PACKETS         (RAW_LOG_PACKETS_PER_DATA_BLOCK_V3 * RAW_LOG_RECOMMENDED_BURST_BLOCKS)

typedef enum
{
    RAW_LOG_OK = 0,
    RAW_LOG_ERR_PARAM = -1,
    RAW_LOG_ERR_MAGIC = -2,
    RAW_LOG_ERR_VERSION = -3,
    RAW_LOG_ERR_SIZE = -4,
    RAW_LOG_ERR_RANGE = -5,
    RAW_LOG_ERR_CRC = -6,
    RAW_LOG_ERR_TAIL = -7
} raw_log_result_t;

typedef struct
{
    uint32_t block_size;
    uint32_t superblock_ring_start_lba;
    uint32_t superblock_ring_count;
    uint32_t data_start_lba;
    uint32_t superblock_write_interval;
    uint32_t stall_threshold_ms;
} raw_log_config_t;

typedef struct
{
    uint32_t card_block_count;
    uint32_t boot_count;
    uint32_t write_seq;
    uint32_t next_data_lba;
    uint32_t last_written_lba;
    uint32_t superblock_ring_index;

    /*
     * Timing fields are "last completed cycle" timing.
     * This keeps logging single-write only; no rewrite is needed just to store timing.
     */
    uint32_t last_data_write_ms;
    uint32_t last_superblock_write_ms;
    uint32_t last_total_write_ms;
    uint32_t max_total_write_ms;
    uint32_t stall_count;
} raw_log_state_t;

#pragma pack(push, 1)
typedef struct
{
    char     magic[8];
    uint32_t version;
    uint32_t block_size;
    uint32_t card_block_count;

    uint32_t superblock_ring_start_lba;
    uint32_t superblock_ring_count;
    uint32_t data_start_lba;
    uint32_t superblock_write_interval;

    uint32_t boot_count;
    uint32_t write_seq;

    uint32_t last_written_lba;
    uint32_t next_data_lba;

    uint32_t uptime_ms;
    uint32_t last_data_write_ms;
    uint32_t last_superblock_write_ms;
    uint32_t last_total_write_ms;
    uint32_t max_total_write_ms;
    uint32_t stall_count;

    uint32_t block_crc32;
    uint8_t  reserved[428];
    uint32_t tail_magic;
} raw_log_superblock_v3_t;
#pragma pack(pop)

typedef char raw_log_superblock_v3_size_check[
    (sizeof(raw_log_superblock_v3_t) == RAW_SD_BLOCK_SIZE) ? 1 : -1];

#pragma pack(push, 1)
typedef struct
{
    char     magic[8];
    uint32_t version;
    uint32_t seq;
    uint32_t lba;
    uint32_t boot_count;
    uint32_t tick_ms;

    uint32_t prev_data_ms;
    uint32_t prev_superblock_ms;
    uint32_t prev_total_ms;

    uint32_t packet_bytes;
    uint32_t packet_count;
    uint32_t payload_bytes;
    uint32_t payload_crc32;
    uint32_t block_crc32;
    uint32_t flags;

    uint8_t  payload[RAW_LOG_DATA_PAYLOAD_BYTES_V3];
} raw_log_data_block_v3_t;
#pragma pack(pop)

typedef char raw_log_data_block_v3_size_check[
    (sizeof(raw_log_data_block_v3_t) == RAW_SD_BLOCK_SIZE) ? 1 : -1];

void raw_log_default_config(raw_log_config_t *cfg);

void raw_log_state_reset(raw_log_state_t *state,
                         uint32_t card_block_count,
                         uint32_t data_start_lba);

void raw_log_state_resume_from_superblock(raw_log_state_t *state,
                                          const raw_log_config_t *cfg,
                                          const raw_log_superblock_v3_t *sb,
                                          uint32_t latest_superblock_lba);

uint32_t raw_log_wrap_data_lba(uint32_t lba,
                               const raw_log_config_t *cfg,
                               uint32_t card_block_count);

int raw_log_seq_is_newer(uint32_t candidate_seq,
                         uint32_t reference_seq);

uint32_t raw_log_get_superblock_lba(const raw_log_config_t *cfg,
                                    const raw_log_state_t *state);

int raw_log_should_write_superblock(const raw_log_config_t *cfg,
                                    const raw_log_state_t *state);

void raw_log_on_write_complete(raw_log_state_t *state,
                               const raw_log_config_t *cfg,
                               uint32_t written_lba,
                               uint32_t data_ms,
                               uint32_t superblock_ms,
                               uint32_t total_ms,
                               int superblock_written);

void raw_log_build_superblock_v3(raw_log_superblock_v3_t *sb,
                                 const raw_log_config_t *cfg,
                                 const raw_log_state_t *state,
                                 uint32_t uptime_ms);

raw_log_result_t raw_log_validate_superblock_v3(const raw_log_superblock_v3_t *sb,
                                                const raw_log_config_t *cfg,
                                                uint32_t card_block_count);

void raw_log_build_data_block_v3(raw_log_data_block_v3_t *db,
                                 const raw_log_state_t *state,
                                 uint32_t lba,
                                 uint32_t tick_ms,
                                 const void *payload,
                                 uint32_t payload_bytes);

raw_log_result_t raw_log_validate_data_block_v3(const raw_log_data_block_v3_t *db);

#ifdef __cplusplus
}
#endif

#endif /* RAW_LOG_CORE_H */
