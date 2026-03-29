#include "raw_log_core.h"
#include "raw_crc32.h"

#include <string.h>

static uint32_t raw_log_superblock_block_crc32(const raw_log_superblock_v3_t *sb)
{
    raw_log_superblock_v3_t temp;

    memcpy(&temp, sb, sizeof(temp));
    temp.block_crc32 = 0U;

    return raw_crc32_compute(&temp, sizeof(temp));
}

static uint32_t raw_log_data_block_crc32(const raw_log_data_block_v3_t *db)
{
    raw_log_data_block_v3_t temp;

    memcpy(&temp, db, sizeof(temp));
    temp.block_crc32 = 0U;

    return raw_crc32_compute(&temp, sizeof(temp));
}

void raw_log_default_config(raw_log_config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    cfg->block_size = RAW_SD_BLOCK_SIZE;
    cfg->superblock_ring_start_lba = 0U;
    cfg->superblock_ring_count = 32U;
    cfg->data_start_lba = 32U;
    cfg->superblock_write_interval = 16U;
    cfg->stall_threshold_ms = 20U;
}

void raw_log_state_reset(raw_log_state_t *state,
                         uint32_t card_block_count,
                         uint32_t data_start_lba)
{
    if (state == NULL)
    {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->card_block_count = card_block_count;
    state->boot_count = 1U;
    state->write_seq = 0U;
    state->next_data_lba = data_start_lba;
    state->last_written_lba = 0U;
    state->superblock_ring_index = 0U;
}

void raw_log_state_resume_from_superblock(raw_log_state_t *state,
                                          const raw_log_config_t *cfg,
                                          const raw_log_superblock_v3_t *sb,
                                          uint32_t latest_superblock_lba)
{
    if ((state == NULL) || (cfg == NULL) || (sb == NULL))
    {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->card_block_count = sb->card_block_count;
    state->boot_count = sb->boot_count + 1U;
    state->write_seq = sb->write_seq + 1U;
    state->next_data_lba = raw_log_wrap_data_lba(sb->next_data_lba,
                                                 cfg,
                                                 sb->card_block_count);
    state->last_written_lba = sb->last_written_lba;
    state->superblock_ring_index =
        ((latest_superblock_lba - cfg->superblock_ring_start_lba + 1U) %
         cfg->superblock_ring_count);

    state->last_data_write_ms = sb->last_data_write_ms;
    state->last_superblock_write_ms = sb->last_superblock_write_ms;
    state->last_total_write_ms = sb->last_total_write_ms;
    state->max_total_write_ms = sb->max_total_write_ms;
    state->stall_count = sb->stall_count;
}

uint32_t raw_log_wrap_data_lba(uint32_t lba,
                               const raw_log_config_t *cfg,
                               uint32_t card_block_count)
{
    if (cfg == NULL)
    {
        return lba;
    }

    if (lba < cfg->data_start_lba)
    {
        return cfg->data_start_lba;
    }

    if (lba >= card_block_count)
    {
        return cfg->data_start_lba;
    }

    return lba;
}

int raw_log_seq_is_newer(uint32_t candidate_seq,
                         uint32_t reference_seq)
{
    return (((int32_t)(candidate_seq - reference_seq)) > 0) ? 1 : 0;
}

uint32_t raw_log_get_superblock_lba(const raw_log_config_t *cfg,
                                    const raw_log_state_t *state)
{
    if ((cfg == NULL) || (state == NULL) || (cfg->superblock_ring_count == 0U))
    {
        return 0U;
    }

    return (cfg->superblock_ring_start_lba + state->superblock_ring_index);
}

int raw_log_should_write_superblock(const raw_log_config_t *cfg,
                                    const raw_log_state_t *state)
{
    if ((cfg == NULL) || (state == NULL) || (cfg->superblock_write_interval == 0U))
    {
        return 0;
    }

    return ((((state->write_seq + 1U) % cfg->superblock_write_interval) == 0U) ? 1 : 0);
}

void raw_log_on_write_complete(raw_log_state_t *state,
                               const raw_log_config_t *cfg,
                               uint32_t written_lba,
                               uint32_t data_ms,
                               uint32_t superblock_ms,
                               uint32_t total_ms,
                               int superblock_written)
{
    if ((state == NULL) || (cfg == NULL))
    {
        return;
    }

    state->last_written_lba = written_lba;
    state->next_data_lba = raw_log_wrap_data_lba(written_lba + 1U,
                                                 cfg,
                                                 state->card_block_count);

    state->last_data_write_ms = data_ms;
    state->last_superblock_write_ms = superblock_ms;
    state->last_total_write_ms = total_ms;

    if (total_ms > state->max_total_write_ms)
    {
        state->max_total_write_ms = total_ms;
    }

    if (total_ms >= cfg->stall_threshold_ms)
    {
        state->stall_count++;
    }

    if ((superblock_written != 0) && (cfg->superblock_ring_count != 0U))
    {
        state->superblock_ring_index++;
        if (state->superblock_ring_index >= cfg->superblock_ring_count)
        {
            state->superblock_ring_index = 0U;
        }
    }

    state->write_seq++;
}

void raw_log_build_superblock_v3(raw_log_superblock_v3_t *sb,
                                 const raw_log_config_t *cfg,
                                 const raw_log_state_t *state,
                                 uint32_t uptime_ms)
{
    if ((sb == NULL) || (cfg == NULL) || (state == NULL))
    {
        return;
    }

    memset(sb, 0, sizeof(*sb));

    memcpy(sb->magic, RAW_LOG_SUPER_MAGIC_V3, sizeof(sb->magic));
    sb->version = RAW_LOG_LAYOUT_VERSION_V3;
    sb->block_size = cfg->block_size;
    sb->card_block_count = state->card_block_count;

    sb->superblock_ring_start_lba = cfg->superblock_ring_start_lba;
    sb->superblock_ring_count = cfg->superblock_ring_count;
    sb->data_start_lba = cfg->data_start_lba;
    sb->superblock_write_interval = cfg->superblock_write_interval;

    sb->boot_count = state->boot_count;
    sb->write_seq = state->write_seq;
    sb->last_written_lba = state->last_written_lba;
    sb->next_data_lba = state->next_data_lba;

    sb->uptime_ms = uptime_ms;
    sb->last_data_write_ms = state->last_data_write_ms;
    sb->last_superblock_write_ms = state->last_superblock_write_ms;
    sb->last_total_write_ms = state->last_total_write_ms;
    sb->max_total_write_ms = state->max_total_write_ms;
    sb->stall_count = state->stall_count;

    sb->tail_magic = RAW_LOG_TAIL_MAGIC_V3;
    sb->block_crc32 = raw_log_superblock_block_crc32(sb);
}

raw_log_result_t raw_log_validate_superblock_v3(const raw_log_superblock_v3_t *sb,
                                                const raw_log_config_t *cfg,
                                                uint32_t card_block_count)
{
    if ((sb == NULL) || (cfg == NULL))
    {
        return RAW_LOG_ERR_PARAM;
    }

    if (memcmp(sb->magic, RAW_LOG_SUPER_MAGIC_V3, sizeof(sb->magic)) != 0)
    {
        return RAW_LOG_ERR_MAGIC;
    }

    if (sb->version != RAW_LOG_LAYOUT_VERSION_V3)
    {
        return RAW_LOG_ERR_VERSION;
    }

    if (sb->block_size != cfg->block_size)
    {
        return RAW_LOG_ERR_SIZE;
    }

    if (sb->card_block_count != card_block_count)
    {
        return RAW_LOG_ERR_RANGE;
    }

    if (sb->superblock_ring_start_lba != cfg->superblock_ring_start_lba)
    {
        return RAW_LOG_ERR_RANGE;
    }

    if (sb->superblock_ring_count != cfg->superblock_ring_count)
    {
        return RAW_LOG_ERR_RANGE;
    }

    if (sb->data_start_lba != cfg->data_start_lba)
    {
        return RAW_LOG_ERR_RANGE;
    }

    if ((sb->next_data_lba < sb->data_start_lba) || (sb->next_data_lba >= sb->card_block_count))
    {
        return RAW_LOG_ERR_RANGE;
    }

    if (sb->tail_magic != RAW_LOG_TAIL_MAGIC_V3)
    {
        return RAW_LOG_ERR_TAIL;
    }

    if (sb->block_crc32 != raw_log_superblock_block_crc32(sb))
    {
        return RAW_LOG_ERR_CRC;
    }

    return RAW_LOG_OK;
}

void raw_log_build_data_block_v3(raw_log_data_block_v3_t *db,
                                 const raw_log_state_t *state,
                                 uint32_t lba,
                                 uint32_t tick_ms,
                                 const void *payload,
                                 uint32_t payload_bytes)
{
    if ((db == NULL) || (state == NULL))
    {
        return;
    }

    memset(db, 0, sizeof(*db));

    memcpy(db->magic, RAW_LOG_DATA_MAGIC_V3, sizeof(db->magic));
    db->version = RAW_LOG_LAYOUT_VERSION_V3;
    db->seq = state->write_seq;
    db->lba = lba;
    db->boot_count = state->boot_count;
    db->tick_ms = tick_ms;

    db->prev_data_ms = state->last_data_write_ms;
    db->prev_superblock_ms = state->last_superblock_write_ms;
    db->prev_total_ms = state->last_total_write_ms;

    db->packet_bytes = RAW_LOG_SENSOR_PACKET_BYTES;

    if (payload_bytes > RAW_LOG_DATA_PAYLOAD_BYTES_V3)
    {
        payload_bytes = RAW_LOG_DATA_PAYLOAD_BYTES_V3;
    }

    if ((payload != NULL) && (payload_bytes > 0U))
    {
        memcpy(db->payload, payload, payload_bytes);
    }

    db->payload_bytes = payload_bytes;
    db->packet_count = payload_bytes / RAW_LOG_SENSOR_PACKET_BYTES;
    db->payload_crc32 = raw_crc32_compute(db->payload, db->payload_bytes);
    db->flags = 0U;
    db->block_crc32 = raw_log_data_block_crc32(db);
}

raw_log_result_t raw_log_validate_data_block_v3(const raw_log_data_block_v3_t *db)
{
    if (db == NULL)
    {
        return RAW_LOG_ERR_PARAM;
    }

    if (memcmp(db->magic, RAW_LOG_DATA_MAGIC_V3, sizeof(db->magic)) != 0)
    {
        return RAW_LOG_ERR_MAGIC;
    }

    if (db->version != RAW_LOG_LAYOUT_VERSION_V3)
    {
        return RAW_LOG_ERR_VERSION;
    }

    if (db->packet_bytes != RAW_LOG_SENSOR_PACKET_BYTES)
    {
        return RAW_LOG_ERR_SIZE;
    }

    if (db->packet_count > RAW_LOG_PACKETS_PER_DATA_BLOCK_V3)
    {
        return RAW_LOG_ERR_SIZE;
    }

    if (db->payload_bytes > RAW_LOG_DATA_PAYLOAD_BYTES_V3)
    {
        return RAW_LOG_ERR_SIZE;
    }

    if (db->payload_crc32 != raw_crc32_compute(db->payload, db->payload_bytes))
    {
        return RAW_LOG_ERR_CRC;
    }

    if (db->block_crc32 != raw_log_data_block_crc32(db))
    {
        return RAW_LOG_ERR_CRC;
    }

    return RAW_LOG_OK;
}
