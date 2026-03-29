#include "raw_log_writer.h"

#include <string.h>

static int raw_log_writer_find_latest_superblock(raw_log_writer_t *writer,
                                                 raw_log_superblock_v3_t *latest,
                                                 uint32_t *latest_lba)
{
    uint32_t lba;
    int found = 0;

    if ((writer == NULL) || (latest == NULL) || (latest_lba == NULL))
    {
        return 0;
    }

    for (lba = writer->cfg.superblock_ring_start_lba;
         lba < (writer->cfg.superblock_ring_start_lba + writer->cfg.superblock_ring_count);
         lba++)
    {
        if (raw_sd_read_blocks(lba, writer->rx_block, 1U) != RAW_SD_OK)
        {
            return -1;
        }

        if (raw_log_validate_superblock_v3((const raw_log_superblock_v3_t *)writer->rx_block,
                                           &writer->cfg,
                                           writer->card_info.LogBlockNbr) == RAW_LOG_OK)
        {
            if ((!found) ||
                raw_log_seq_is_newer(((const raw_log_superblock_v3_t *)writer->rx_block)->write_seq,
                                     latest->write_seq))
            {
                memcpy(latest, writer->rx_block, RAW_SD_BLOCK_SIZE);
                *latest_lba = lba;
                found = 1;
            }
        }
    }

    return found;
}

void raw_log_writer_init_defaults(raw_log_writer_t *writer)
{
    if (writer == NULL)
    {
        return;
    }

    memset(writer, 0, sizeof(*writer));
    raw_log_default_config(&writer->cfg);
    writer->log_every_n_blocks = 64U;
}

raw_log_writer_result_t raw_log_writer_begin(raw_log_writer_t *writer)
{
    raw_log_superblock_v3_t latest;
    uint32_t latest_lba = 0U;
    int recover_result;

    if (writer == NULL)
    {
        return RAW_LOG_WRITER_ERR_PARAM;
    }

    memset(&writer->card_info, 0, sizeof(writer->card_info));
    raw_sd_get_card_info(&writer->card_info);

    if (writer->card_info.LogBlockNbr == 0U)
    {
        return RAW_LOG_WRITER_ERR_SD_INFO;
    }

    if (writer->card_info.LogBlockNbr <= writer->cfg.data_start_lba)
    {
        return RAW_LOG_WRITER_ERR_LAYOUT;
    }

    raw_log_state_reset(&writer->state,
                        writer->card_info.LogBlockNbr,
                        writer->cfg.data_start_lba);

    memset(&latest, 0, sizeof(latest));
    recover_result = raw_log_writer_find_latest_superblock(writer, &latest, &latest_lba);

    if (recover_result < 0)
    {
        return RAW_LOG_WRITER_ERR_SD_READ;
    }

    if (recover_result > 0)
    {
        raw_log_state_resume_from_superblock(&writer->state,
                                             &writer->cfg,
                                             &latest,
                                             latest_lba);
    }

    return RAW_LOG_WRITER_OK;
}

raw_log_writer_result_t raw_log_writer_write_payload(raw_log_writer_t *writer,
                                                     const void *payload,
                                                     uint32_t payload_bytes,
                                                     raw_log_writer_step_info_t *step_info)
{
    raw_log_data_block_v3_t *db;
    raw_log_superblock_v3_t *sb;
    raw_log_state_t checkpoint_state;
    uint32_t data_lba;
    uint32_t tick_ms;
    uint32_t data_start_ms;
    uint32_t data_end_ms;
    uint32_t sb_start_ms = 0U;
    uint32_t sb_end_ms = 0U;
    uint32_t data_ms;
    uint32_t superblock_ms = 0U;
    uint32_t total_ms;
    uint32_t superblock_lba = 0U;
    int superblock_written = 0;
    int do_superblock_write;

    if ((writer == NULL) || ((payload == NULL) && (payload_bytes != 0U)))
    {
        return RAW_LOG_WRITER_ERR_PARAM;
    }

    if (payload_bytes > RAW_LOG_DATA_PAYLOAD_BYTES_V3)
    {
        return RAW_LOG_WRITER_ERR_PARAM;
    }

    data_lba = writer->state.next_data_lba;
    tick_ms = HAL_GetTick();
    do_superblock_write = raw_log_should_write_superblock(&writer->cfg, &writer->state);

    if (step_info != NULL)
    {
        memset(step_info, 0, sizeof(*step_info));
        step_info->seq = writer->state.write_seq;
        step_info->data_lba = data_lba;
        step_info->next_data_lba = raw_log_wrap_data_lba(data_lba + 1U,
                                                         &writer->cfg,
                                                         writer->state.card_block_count);
        step_info->superblock_lba = raw_log_get_superblock_lba(&writer->cfg, &writer->state);
    }

    db = (raw_log_data_block_v3_t *)writer->tx_block;
    raw_log_build_data_block_v3(db,
                                &writer->state,
                                data_lba,
                                tick_ms,
                                payload,
                                payload_bytes);

    data_start_ms = HAL_GetTick();
    if (raw_sd_write_blocks(data_lba, writer->tx_block, 1U) != RAW_SD_OK)
    {
        return RAW_LOG_WRITER_ERR_SD_WRITE;
    }
    data_end_ms = HAL_GetTick();
    data_ms = data_end_ms - data_start_ms;

    if (do_superblock_write)
    {
        superblock_lba = raw_log_get_superblock_lba(&writer->cfg, &writer->state);

        checkpoint_state = writer->state;
        checkpoint_state.last_written_lba = data_lba;
        checkpoint_state.next_data_lba = raw_log_wrap_data_lba(data_lba + 1U,
                                                               &writer->cfg,
                                                               writer->state.card_block_count);

        sb = (raw_log_superblock_v3_t *)writer->tx_block;
        raw_log_build_superblock_v3(sb,
                                    &writer->cfg,
                                    &checkpoint_state,
                                    tick_ms);

        sb_start_ms = HAL_GetTick();
        if (raw_sd_write_blocks(superblock_lba, writer->tx_block, 1U) != RAW_SD_OK)
        {
            return RAW_LOG_WRITER_ERR_SD_WRITE;
        }
        sb_end_ms = HAL_GetTick();
        superblock_ms = sb_end_ms - sb_start_ms;
        superblock_written = 1;
    }

    total_ms = data_ms + superblock_ms;

    if (step_info != NULL)
    {
        step_info->seq = writer->state.write_seq;
        step_info->data_lba = data_lba;
        step_info->next_data_lba = raw_log_wrap_data_lba(data_lba + 1U,
                                                         &writer->cfg,
                                                         writer->state.card_block_count);
        step_info->superblock_lba = superblock_lba;
        step_info->data_ms = data_ms;
        step_info->superblock_ms = superblock_ms;
        step_info->total_ms = total_ms;
        step_info->stall_count = writer->state.stall_count +
                                 ((total_ms >= writer->cfg.stall_threshold_ms) ? 1U : 0U);
        step_info->superblock_written = (uint8_t)superblock_written;
        step_info->stall = (uint8_t)((total_ms >= writer->cfg.stall_threshold_ms) ? 1U : 0U);
    }

    raw_log_on_write_complete(&writer->state,
                              &writer->cfg,
                              data_lba,
                              data_ms,
                              superblock_ms,
                              total_ms,
                              superblock_written);

    return RAW_LOG_WRITER_OK;
}

void raw_log_writer_fill_test_payload(void *payload,
                                      uint32_t payload_bytes,
                                      uint32_t seq,
                                      uint32_t lba)
{
    uint8_t *dst = (uint8_t *)payload;
    uint32_t i;

    if (dst == NULL)
    {
        return;
    }

    for (i = 0U; i < payload_bytes; i++)
    {
        dst[i] = (uint8_t)((seq + lba + i) & 0xFFU);
    }
}
