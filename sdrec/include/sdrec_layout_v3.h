#ifndef SDREC_LAYOUT_V3_H
#define SDREC_LAYOUT_V3_H

#include "sdrec_build_config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDREC_LAYOUT_VERSION_V3                 3U
#define SDREC_SUPER_MAGIC_V3                    "RAWSDIO3"
#define SDREC_DATA_MAGIC_V3                     "RAWDATA3"
#define SDREC_TAIL_MAGIC_V3                     0xA55A5AA5UL

/*
 * Packet payload is always made of 64-byte source packets.
 * A 512-byte SD block contains 64-byte header + 448-byte payload = 7 packets.
 */
#define SDREC_PACKET_BYTES               64U
#define SDREC_DATA_HEADER_BYTES_V3              64U
#define SDREC_DATA_PAYLOAD_BYTES_V3             (SDREC_CARD_BLOCK_SIZE - SDREC_DATA_HEADER_BYTES_V3)
#define SDREC_PACKETS_PER_BLOCK_V3         (SDREC_DATA_PAYLOAD_BYTES_V3 / SDREC_PACKET_BYTES)
#define SDREC_RECOMMENDED_BURST_BLOCKS          8U
#define SDREC_RECOMMENDED_BURST_BYTES           (SDREC_RECOMMENDED_BURST_BLOCKS * SDREC_CARD_BLOCK_SIZE)
#define SDREC_RECOMMENDED_BURST_PACKETS         (SDREC_PACKETS_PER_BLOCK_V3 * SDREC_RECOMMENDED_BURST_BLOCKS)

#define SDREC_FLAG_PAYLOAD_LINEAR_TEST          0x00000001UL
#define SDREC_FLAG_PAYLOAD_DUMMY_SENSOR         0x00000002UL
#define SDREC_FLAG_PAYLOAD_SENSOR_DMA           0x00000004UL
#define SDREC_FLAG_PAYLOAD_TABLE_SENSOR         0x00000008UL

#define SDREC_DUMMY_CPU_PACKET_MAGIC                0x44554D59UL /* "DUMY" */
#define SDREC_WAVE_PACKET_MAGIC                0x53494E45UL /* "SINE" */
#define SDREC_WAVE_PACKET_FMT_S16    1UL
#define SDREC_WAVE_PACKET_MAX_SAMPLES          20U

typedef enum
{
    SDREC_LAYOUT_OK = 0,
    SDREC_LAYOUT_ERR_PARAM = -1,
    SDREC_LAYOUT_ERR_MAGIC = -2,
    SDREC_LAYOUT_ERR_VERSION = -3,
    SDREC_LAYOUT_ERR_SIZE = -4,
    SDREC_LAYOUT_ERR_RANGE = -5,
    SDREC_LAYOUT_ERR_CRC = -6,
    SDREC_LAYOUT_ERR_TAIL = -7
} sdrec_layout_status_t;

typedef struct
{
    uint32_t block_size;
    uint32_t superblock_ring_start_lba;
    uint32_t superblock_ring_count;
    uint32_t data_start_lba;
    uint32_t superblock_write_interval;
    uint32_t stall_threshold_ms;
} sdrec_layout_cfg_t;

typedef struct
{
    uint32_t card_block_count;
    uint32_t boot_count;
    uint32_t write_seq;
    uint32_t next_data_lba;
    uint32_t last_written_lba;
    uint32_t superblock_ring_index;

    uint32_t last_data_write_ms;
    uint32_t last_superblock_write_ms;
    uint32_t last_total_write_ms;
    uint32_t max_total_write_ms;
    uint32_t stall_count;
} sdrec_layout_state_t;

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
} sdrec_superblock_v3_t;
#pragma pack(pop)

typedef char sdrec_superblock_v3_size_check[
    (sizeof(sdrec_superblock_v3_t) == SDREC_CARD_BLOCK_SIZE) ? 1 : -1];

#pragma pack(push, 1)
typedef struct
{
    uint32_t magic;
    uint32_t source_packet_seq;
    uint32_t tick_ms;
    uint32_t sink_seq_snapshot;
    uint32_t ready_slots_snapshot;
    uint32_t dropped_packets_snapshot;
    uint32_t committed_blocks_snapshot;
    uint32_t reserved0;
    uint8_t  tail[32];
} sdrec_dummy_cpu_packet_t;
#pragma pack(pop)

typedef char sdrec_dummy_cpu_packet_size_check[
    (sizeof(sdrec_dummy_cpu_packet_t) == SDREC_PACKET_BYTES) ? 1 : -1];

#pragma pack(push, 1)
typedef struct
{
    uint32_t magic;
    uint32_t source_packet_seq;
    uint32_t tick_ms;
    uint32_t table_index;
    uint32_t sample_count;
    uint32_t sample_format;
    int16_t  samples[SDREC_WAVE_PACKET_MAX_SAMPLES];
} sdrec_wave_packet_t;
#pragma pack(pop)

typedef char sdrec_wave_packet_size_check[
    (sizeof(sdrec_wave_packet_t) == SDREC_PACKET_BYTES) ? 1 : -1];

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

    uint8_t  payload[SDREC_DATA_PAYLOAD_BYTES_V3];
} sdrec_data_block_v3_t;
#pragma pack(pop)

typedef char sdrec_data_block_v3_size_check[
    (sizeof(sdrec_data_block_v3_t) == SDREC_CARD_BLOCK_SIZE) ? 1 : -1];

void sdrec_layout_cfg_init_defaults(sdrec_layout_cfg_t *cfg);

void sdrec_layout_state_reset(sdrec_layout_state_t *state,
                         uint32_t card_block_count,
                         uint32_t data_start_lba);

void sdrec_layout_state_restore_from_superblock(sdrec_layout_state_t *state,
                                          const sdrec_layout_cfg_t *cfg,
                                          const sdrec_superblock_v3_t *sb,
                                          uint32_t latest_superblock_lba);

uint32_t sdrec_layout_wrap_lba(uint32_t lba,
                               const sdrec_layout_cfg_t *cfg,
                               uint32_t card_block_count);

int sdrec_layout_is_newer_seq(uint32_t candidate_seq,
                         uint32_t reference_seq);

uint32_t sdrec_layout_current_superblock_lba(const sdrec_layout_cfg_t *cfg,
                                    const sdrec_layout_state_t *state);

int sdrec_layout_should_checkpoint(const sdrec_layout_cfg_t *cfg,
                                    const sdrec_layout_state_t *state);

void sdrec_layout_note_commit(sdrec_layout_state_t *state,
                               const sdrec_layout_cfg_t *cfg,
                               uint32_t written_lba,
                               uint32_t data_ms,
                               uint32_t superblock_ms,
                               uint32_t total_ms,
                               int superblock_written);

void sdrec_pack_superblock_v3(sdrec_superblock_v3_t *sb,
                                 const sdrec_layout_cfg_t *cfg,
                                 const sdrec_layout_state_t *state,
                                 uint32_t uptime_ms);

sdrec_layout_status_t sdrec_check_superblock_v3(const sdrec_superblock_v3_t *sb,
                                                const sdrec_layout_cfg_t *cfg,
                                                uint32_t card_block_count);

void sdrec_pack_data_block_v3(sdrec_data_block_v3_t *db,
                                 const sdrec_layout_state_t *state,
                                 uint32_t lba,
                                 uint32_t tick_ms,
                                 const void *payload,
                                 uint32_t payload_bytes,
                                 uint32_t flags);

sdrec_layout_status_t sdrec_check_data_block_v3(const sdrec_data_block_v3_t *db);

#ifdef __cplusplus
}
#endif

#endif /* SDREC_LAYOUT_V3_H */
