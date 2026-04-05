#include "sdrec_layout_v3.h"
#include "sdrec_crc32.h"

#include <string.h>

static uint32_t sdrec_superblock_block_crc32(const sdrec_superblock_v3_t *sb)
{
    sdrec_superblock_v3_t temp;

    memcpy(&temp, sb, sizeof(temp));
    temp.block_crc32 = 0U;

    return sdrec_crc32_compute(&temp, sizeof(temp));
}

static uint32_t sdrec_data_block_crc32(const sdrec_data_block_v3_t *db)
{
    sdrec_data_block_v3_t temp;

    memcpy(&temp, db, sizeof(temp));
    temp.block_crc32 = 0U;

    return sdrec_crc32_compute(&temp, sizeof(temp));
}

void sdrec_layout_cfg_init_defaults(sdrec_layout_cfg_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    cfg->block_size = SDREC_CARD_BLOCK_SIZE;
    cfg->superblock_ring_start_lba = 0U;
    cfg->superblock_ring_count = 32U;
    cfg->data_start_lba = 32U;
    cfg->superblock_write_interval = 16U;
    cfg->stall_threshold_ms = 20U;
}

void sdrec_layout_state_reset(sdrec_layout_state_t *state,
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

void sdrec_layout_state_restore_from_superblock(sdrec_layout_state_t *state,
                                          const sdrec_layout_cfg_t *cfg,
                                          const sdrec_superblock_v3_t *sb,
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
    state->next_data_lba = sdrec_layout_wrap_lba(sb->next_data_lba,
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

uint32_t sdrec_layout_wrap_lba(uint32_t lba,
                               const sdrec_layout_cfg_t *cfg,
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

int sdrec_layout_is_newer_seq(uint32_t candidate_seq,
                         uint32_t reference_seq)
{
    return (((int32_t)(candidate_seq - reference_seq)) > 0) ? 1 : 0;
}

uint32_t sdrec_layout_current_superblock_lba(const sdrec_layout_cfg_t *cfg,
                                    const sdrec_layout_state_t *state)
{
    if ((cfg == NULL) || (state == NULL) || (cfg->superblock_ring_count == 0U))
    {
        return 0U;
    }

    return (cfg->superblock_ring_start_lba + state->superblock_ring_index);
}

int sdrec_layout_should_checkpoint(const sdrec_layout_cfg_t *cfg,
                                    const sdrec_layout_state_t *state)
{
    if ((cfg == NULL) || (state == NULL) || (cfg->superblock_write_interval == 0U))
    {
        return 0;
    }

    return ((((state->write_seq + 1U) % cfg->superblock_write_interval) == 0U) ? 1 : 0);
}

void sdrec_layout_note_commit(sdrec_layout_state_t *state,
                               const sdrec_layout_cfg_t *cfg,
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
    state->next_data_lba = sdrec_layout_wrap_lba(written_lba + 1U,
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

void sdrec_pack_superblock_v3(sdrec_superblock_v3_t *sb,
                                 const sdrec_layout_cfg_t *cfg,
                                 const sdrec_layout_state_t *state,
                                 uint32_t uptime_ms)
{
    if ((sb == NULL) || (cfg == NULL) || (state == NULL))
    {
        return;
    }

    memset(sb, 0, sizeof(*sb));

    memcpy(sb->magic, SDREC_SUPER_MAGIC_V3, sizeof(sb->magic));
    sb->version = SDREC_LAYOUT_VERSION_V3;
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

    sb->tail_magic = SDREC_TAIL_MAGIC_V3;
    sb->block_crc32 = sdrec_superblock_block_crc32(sb);
}

sdrec_layout_status_t sdrec_check_superblock_v3(const sdrec_superblock_v3_t *sb,
                                                const sdrec_layout_cfg_t *cfg,
                                                uint32_t card_block_count)
{
    if ((sb == NULL) || (cfg == NULL))
    {
        return SDREC_LAYOUT_ERR_PARAM;
    }

    if (memcmp(sb->magic, SDREC_SUPER_MAGIC_V3, sizeof(sb->magic)) != 0)
    {
        return SDREC_LAYOUT_ERR_MAGIC;
    }

    if (sb->version != SDREC_LAYOUT_VERSION_V3)
    {
        return SDREC_LAYOUT_ERR_VERSION;
    }

    if (sb->block_size != cfg->block_size)
    {
        return SDREC_LAYOUT_ERR_SIZE;
    }

    if (sb->card_block_count != card_block_count)
    {
        return SDREC_LAYOUT_ERR_RANGE;
    }

    if (sb->superblock_ring_start_lba != cfg->superblock_ring_start_lba)
    {
        return SDREC_LAYOUT_ERR_RANGE;
    }

    if (sb->superblock_ring_count != cfg->superblock_ring_count)
    {
        return SDREC_LAYOUT_ERR_RANGE;
    }

    if (sb->data_start_lba != cfg->data_start_lba)
    {
        return SDREC_LAYOUT_ERR_RANGE;
    }

    if ((sb->next_data_lba < sb->data_start_lba) || (sb->next_data_lba >= sb->card_block_count))
    {
        return SDREC_LAYOUT_ERR_RANGE;
    }

    if (sb->tail_magic != SDREC_TAIL_MAGIC_V3)
    {
        return SDREC_LAYOUT_ERR_TAIL;
    }

    if (sb->block_crc32 != sdrec_superblock_block_crc32(sb))
    {
        return SDREC_LAYOUT_ERR_CRC;
    }

    return SDREC_LAYOUT_OK;
}

void sdrec_pack_data_block_v3(sdrec_data_block_v3_t *db,
                                 const sdrec_layout_state_t *state,
                                 uint32_t lba,
                                 uint32_t tick_ms,
                                 const void *payload,
                                 uint32_t payload_bytes,
                                 uint32_t flags)
{
    if ((db == NULL) || (state == NULL))
    {
        return;
    }

    memset(db, 0, sizeof(*db));

    memcpy(db->magic, SDREC_DATA_MAGIC_V3, sizeof(db->magic));
    db->version = SDREC_LAYOUT_VERSION_V3;
    db->seq = state->write_seq;
    db->lba = lba;
    db->boot_count = state->boot_count;
    db->tick_ms = tick_ms;

    db->prev_data_ms = state->last_data_write_ms;
    db->prev_superblock_ms = state->last_superblock_write_ms;
    db->prev_total_ms = state->last_total_write_ms;

    db->packet_bytes = SDREC_PACKET_BYTES;

    if (payload_bytes > SDREC_DATA_PAYLOAD_BYTES_V3)
    {
        payload_bytes = SDREC_DATA_PAYLOAD_BYTES_V3;
    }

    if ((payload != NULL) && (payload_bytes > 0U))
    {
        memcpy(db->payload, payload, payload_bytes);
    }

    db->payload_bytes = payload_bytes;
    db->packet_count = payload_bytes / SDREC_PACKET_BYTES;
    db->payload_crc32 = sdrec_crc32_compute(db->payload, db->payload_bytes);
    db->flags = flags;
    db->block_crc32 = sdrec_data_block_crc32(db);
}

sdrec_layout_status_t sdrec_check_data_block_v3(const sdrec_data_block_v3_t *db)
{
    if (db == NULL)
    {
        return SDREC_LAYOUT_ERR_PARAM;
    }

    if (memcmp(db->magic, SDREC_DATA_MAGIC_V3, sizeof(db->magic)) != 0)
    {
        return SDREC_LAYOUT_ERR_MAGIC;
    }

    if (db->version != SDREC_LAYOUT_VERSION_V3)
    {
        return SDREC_LAYOUT_ERR_VERSION;
    }

    if (db->packet_bytes != SDREC_PACKET_BYTES)
    {
        return SDREC_LAYOUT_ERR_SIZE;
    }

    if (db->packet_count > SDREC_PACKETS_PER_BLOCK_V3)
    {
        return SDREC_LAYOUT_ERR_SIZE;
    }

    if (db->payload_bytes > SDREC_DATA_PAYLOAD_BYTES_V3)
    {
        return SDREC_LAYOUT_ERR_SIZE;
    }

    if (db->payload_crc32 != sdrec_crc32_compute(db->payload, db->payload_bytes))
    {
        return SDREC_LAYOUT_ERR_CRC;
    }

    if (db->block_crc32 != sdrec_data_block_crc32(db))
    {
        return SDREC_LAYOUT_ERR_CRC;
    }

    return SDREC_LAYOUT_OK;
}
