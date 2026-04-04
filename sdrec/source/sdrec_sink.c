#include "sdrec_sink.h"

#include <string.h>

static int sdrec_sink_find_latest_superblock(sdrec_sink_t *writer,
                                                 sdrec_superblock_v3_t *latest,
                                                 uint32_t *latest_lba)
{
    uint32_t lba;
    int found = 0;

    if ((writer == NULL) || (latest == NULL) || (latest_lba == NULL))
    {
        return 0;
    }

    for (lba = writer->layout_cfg.superblock_ring_start_lba;
         lba < (writer->layout_cfg.superblock_ring_start_lba + writer->layout_cfg.superblock_ring_count);
         lba++)
    {
        if (sdrec_card_read_blocks(lba, writer->rx_block, 1U) != SDREC_CARD_OK)
        {
            return -1;
        }

        if (sdrec_check_superblock_v3((const sdrec_superblock_v3_t *)writer->rx_block,
                                           &writer->layout_cfg,
                                           writer->card_info.LogBlockNbr) == SDREC_LAYOUT_OK)
        {
            if ((!found) ||
                sdrec_layout_is_newer_seq(((const sdrec_superblock_v3_t *)writer->rx_block)->write_seq,
                                     latest->write_seq))
            {
                memcpy(latest, writer->rx_block, SDREC_CARD_BLOCK_SIZE);
                *latest_lba = lba;
                found = 1;
            }
        }
    }

    return found;
}

static void sdrec_sink_fill_step_info(sdrec_sink_t *writer,
                                          sdrec_commit_report_t *step_info,
                                          uint32_t data_lba,
                                          uint32_t superblock_lba,
                                          uint32_t data_ms,
                                          uint32_t superblock_ms,
                                          int superblock_written)
{
    uint32_t total_ms;

    if ((writer == NULL) || (step_info == NULL))
    {
        return;
    }

    total_ms = data_ms + superblock_ms;

    memset(step_info, 0, sizeof(*step_info));
    step_info->seq = writer->layout_state.write_seq;
    step_info->last_seq = writer->layout_state.write_seq;
    step_info->block_count = 1U;
    step_info->data_lba = data_lba;
    step_info->next_data_lba = sdrec_layout_wrap_lba(data_lba + 1U,
                                                     &writer->layout_cfg,
                                                     writer->layout_state.card_block_count);
    step_info->superblock_lba = superblock_lba;
    step_info->data_ms = data_ms;
    step_info->superblock_ms = superblock_ms;
    step_info->total_ms = total_ms;
    step_info->stall_count = writer->layout_state.stall_count +
                             ((total_ms >= writer->layout_cfg.stall_threshold_ms) ? 1U : 0U);
    step_info->superblock_written = (uint8_t)superblock_written;
    step_info->stall = (uint8_t)((total_ms >= writer->layout_cfg.stall_threshold_ms) ? 1U : 0U);
}

static sdrec_sink_status_t sdrec_sink_complete_write(sdrec_sink_t *writer,
                                                             sdrec_commit_report_t *step_info,
                                                             uint32_t data_lba,
                                                             uint32_t superblock_lba,
                                                             uint32_t data_ms,
                                                             uint32_t superblock_ms,
                                                             int superblock_written)
{
    if (writer == NULL)
    {
        return SDREC_SINK_ERR_PARAM;
    }

    sdrec_sink_fill_step_info(writer,
                                  step_info,
                                  data_lba,
                                  superblock_lba,
                                  data_ms,
                                  superblock_ms,
                                  superblock_written);

    sdrec_layout_note_commit(&writer->layout_state,
                              &writer->layout_cfg,
                              data_lba,
                              data_ms,
                              superblock_ms,
                              (data_ms + superblock_ms),
                              superblock_written);

    return SDREC_SINK_OK;
}


static void sdrec_sink_fill_burst_step_info(sdrec_sink_t *writer,
                                                sdrec_commit_report_t *step_info,
                                                uint32_t first_seq,
                                                uint32_t last_seq,
                                                uint32_t block_count,
                                                uint32_t first_data_lba,
                                                uint32_t superblock_lba,
                                                uint32_t data_ms,
                                                uint32_t superblock_ms,
                                                int superblock_written)
{
    uint32_t total_ms;

    if ((writer == NULL) || (step_info == NULL))
    {
        return;
    }

    total_ms = data_ms + superblock_ms;

    memset(step_info, 0, sizeof(*step_info));
    step_info->seq = first_seq;
    step_info->last_seq = last_seq;
    step_info->block_count = block_count;
    step_info->data_lba = first_data_lba;
    step_info->next_data_lba = writer->layout_state.next_data_lba;
    step_info->superblock_lba = superblock_lba;
    step_info->data_ms = data_ms;
    step_info->superblock_ms = superblock_ms;
    step_info->total_ms = total_ms;
    step_info->stall_count = writer->layout_state.stall_count;
    step_info->superblock_written = (uint8_t)superblock_written;
    step_info->stall = (uint8_t)((total_ms >= writer->layout_cfg.stall_threshold_ms) ? 1U : 0U);
}

static void sdrec_sink_commit_burst_state(sdrec_sink_t *writer,
                                              uint32_t last_data_lba,
                                              uint32_t block_count,
                                              uint32_t data_ms,
                                              uint32_t superblock_ms,
                                              int superblock_written)
{
    if ((writer == NULL) || (block_count == 0U))
    {
        return;
    }

    sdrec_layout_note_commit(&writer->layout_state,
                              &writer->layout_cfg,
                              last_data_lba,
                              data_ms,
                              superblock_ms,
                              (data_ms + superblock_ms),
                              superblock_written);

    if (block_count > 1U)
    {
        writer->layout_state.write_seq += (block_count - 1U);
    }
}

static sdrec_sink_status_t sdrec_sink_finish_async_burst(sdrec_sink_t *writer,
                                                                 sdrec_commit_report_t *step_info,
                                                                 int superblock_written)
{
    uint32_t first_seq;
    uint32_t last_seq;
    uint32_t block_count;
    uint32_t first_data_lba;
    uint32_t last_data_lba;
    uint32_t superblock_lba;
    uint32_t data_ms;
    uint32_t superblock_ms;

    if (writer == NULL)
    {
        return SDREC_SINK_ERR_PARAM;
    }

    first_seq = writer->async.first_seq;
    last_seq = writer->async.last_seq;
    block_count = writer->async.block_count;
    first_data_lba = writer->async.data_lba;
    last_data_lba = writer->async.last_data_lba;
    superblock_lba = (superblock_written != 0) ? writer->async.superblock_lba : 0U;
    data_ms = writer->async.data_ms;
    superblock_ms = (superblock_written != 0) ? writer->async.superblock_ms : 0U;

    sdrec_sink_commit_burst_state(writer,
                                      last_data_lba,
                                      block_count,
                                      data_ms,
                                      superblock_ms,
                                      superblock_written);

    sdrec_sink_fill_burst_step_info(writer,
                                        step_info,
                                        first_seq,
                                        last_seq,
                                        block_count,
                                        first_data_lba,
                                        superblock_lba,
                                        data_ms,
                                        superblock_ms,
                                        superblock_written);

    memset(&writer->async, 0, sizeof(writer->async));
    writer->async.state = SDREC_SINK_ASYNC_IDLE;

    return SDREC_SINK_OK;
}

void sdrec_sink_init_defaults(sdrec_sink_t *writer)
{
    if (writer == NULL)
    {
        return;
    }

    memset(writer, 0, sizeof(*writer));
    sdrec_layout_cfg_init_defaults(&writer->layout_cfg);
    writer->log_every_n_blocks = 64U;
    writer->payload_flags = SDREC_FLAG_PAYLOAD_LINEAR_TEST;
    writer->async.state = SDREC_SINK_ASYNC_IDLE;
}

void sdrec_sink_set_payload_flags(sdrec_sink_t *writer,
                                      uint32_t payload_flags)
{
    if (writer == NULL)
    {
        return;
    }

    writer->payload_flags = payload_flags;
}

void sdrec_sink_set_runtime_config(sdrec_sink_t *writer,
                                       const sdrec_layout_cfg_t *cfg,
                                       uint32_t log_every_n_blocks,
                                       uint32_t payload_flags)
{
    if (writer == NULL)
    {
        return;
    }

    if (cfg != NULL)
    {
        writer->layout_cfg = *cfg;
    }

    writer->log_every_n_blocks = log_every_n_blocks;
    writer->payload_flags = payload_flags;
}

sdrec_sink_status_t sdrec_sink_open(sdrec_sink_t *writer)
{
    sdrec_superblock_v3_t latest;
    uint32_t latest_lba = 0U;
    int recover_result;

    if (writer == NULL)
    {
        return SDREC_SINK_ERR_PARAM;
    }

    memset(&writer->card_info, 0, sizeof(writer->card_info));
    sdrec_card_get_card_info(&writer->card_info);

    if (writer->card_info.LogBlockNbr == 0U)
    {
        return SDREC_SINK_ERR_CARD_INFO;
    }

    if (writer->card_info.LogBlockNbr <= writer->layout_cfg.data_start_lba)
    {
        return SDREC_SINK_ERR_LAYOUT;
    }

    sdrec_layout_state_reset(&writer->layout_state,
                        writer->card_info.LogBlockNbr,
                        writer->layout_cfg.data_start_lba);

    memset(&latest, 0, sizeof(latest));
    recover_result = sdrec_sink_find_latest_superblock(writer, &latest, &latest_lba);

    if (recover_result < 0)
    {
        return SDREC_SINK_ERR_CARD_READ;
    }

    if (recover_result > 0)
    {
        sdrec_layout_state_restore_from_superblock(&writer->layout_state,
                                             &writer->layout_cfg,
                                             &latest,
                                             latest_lba);
    }

    writer->async.state = SDREC_SINK_ASYNC_IDLE;
    writer->async.superblock_needed = 0U;

    return SDREC_SINK_OK;
}

sdrec_sink_status_t sdrec_sink_write_payload(sdrec_sink_t *writer,
                                                     const void *payload,
                                                     uint32_t payload_bytes,
                                                     sdrec_commit_report_t *step_info)
{
    sdrec_data_block_v3_t *db;
    sdrec_superblock_v3_t *sb;
    sdrec_layout_state_t checkpoint_state;
    uint32_t data_lba;
    uint32_t tick_ms;
    uint32_t data_start_ms;
    uint32_t data_end_ms;
    uint32_t sb_start_ms = 0U;
    uint32_t sb_end_ms = 0U;
    uint32_t data_ms;
    uint32_t superblock_ms = 0U;
    uint32_t superblock_lba = 0U;
    int superblock_written = 0;
    int do_superblock_write;

    if ((writer == NULL) || ((payload == NULL) && (payload_bytes != 0U)))
    {
        return SDREC_SINK_ERR_PARAM;
    }

    if (payload_bytes > SDREC_DATA_PAYLOAD_BYTES_V3)
    {
        return SDREC_SINK_ERR_PARAM;
    }

    data_lba = writer->layout_state.next_data_lba;
    tick_ms = HAL_GetTick();
    do_superblock_write = sdrec_layout_should_checkpoint(&writer->layout_cfg, &writer->layout_state);

    db = (sdrec_data_block_v3_t *)writer->tx_block;
    sdrec_pack_data_block_v3(db,
                                &writer->layout_state,
                                data_lba,
                                tick_ms,
                                payload,
                                payload_bytes,
                                writer->payload_flags);

    data_start_ms = HAL_GetTick();
    if (sdrec_card_write_blocks(data_lba, writer->tx_block, 1U) != SDREC_CARD_OK)
    {
        return SDREC_SINK_ERR_CARD_WRITE;
    }
    data_end_ms = HAL_GetTick();
    data_ms = data_end_ms - data_start_ms;

    if (do_superblock_write)
    {
        superblock_lba = sdrec_layout_current_superblock_lba(&writer->layout_cfg, &writer->layout_state);

        checkpoint_state = writer->layout_state;
        checkpoint_state.last_written_lba = data_lba;
        checkpoint_state.next_data_lba = sdrec_layout_wrap_lba(data_lba + 1U,
                                                               &writer->layout_cfg,
                                                               writer->layout_state.card_block_count);

        sb = (sdrec_superblock_v3_t *)writer->tx_block;
        sdrec_pack_superblock_v3(sb,
                                    &writer->layout_cfg,
                                    &checkpoint_state,
                                    tick_ms);

        sb_start_ms = HAL_GetTick();
        if (sdrec_card_write_blocks(superblock_lba, writer->tx_block, 1U) != SDREC_CARD_OK)
        {
            return SDREC_SINK_ERR_CARD_WRITE;
        }
        sb_end_ms = HAL_GetTick();
        superblock_ms = sb_end_ms - sb_start_ms;
        superblock_written = 1;
    }

    return sdrec_sink_complete_write(writer,
                                         step_info,
                                         data_lba,
                                         superblock_lba,
                                         data_ms,
                                         superblock_ms,
                                         superblock_written);
}


int sdrec_sink_async_busy(const sdrec_sink_t *writer)
{
    if (writer == NULL)
    {
        return 0;
    }

    return ((writer->async.state != SDREC_SINK_ASYNC_IDLE) ? 1 : 0);
}

uint32_t sdrec_sink_get_max_contiguous_write_blocks(const sdrec_sink_t *writer)
{
    if (writer == NULL)
    {
        return 0U;
    }

    if ((writer->layout_state.card_block_count <= writer->layout_cfg.data_start_lba) ||
        (writer->layout_state.next_data_lba < writer->layout_cfg.data_start_lba) ||
        (writer->layout_state.next_data_lba >= writer->layout_state.card_block_count))
    {
        return 0U;
    }

    return (writer->layout_state.card_block_count - writer->layout_state.next_data_lba);
}

sdrec_sink_status_t sdrec_sink_begin_async_batch(sdrec_sink_t *writer,
                                                            const void **payloads,
                                                            const uint32_t *payload_bytes,
                                                            uint32_t block_count)
{
    sdrec_data_block_v3_t *db;
    sdrec_layout_state_t build_state;
    sdrec_card_status_t sd_status;
    uint32_t tick_ms;
    uint32_t i;
    uint32_t current_lba;
    uint32_t current_seq;
    uint32_t max_contiguous_blocks;

    if ((writer == NULL) || (payloads == NULL) || (payload_bytes == NULL) ||
        (block_count == 0U) || (block_count > SDREC_SINK_MAX_ASYNC_BLOCKS))
    {
        return SDREC_SINK_ERR_PARAM;
    }

    if (sdrec_sink_async_busy(writer) != 0)
    {
        return SDREC_SINK_ERR_STATE;
    }

    max_contiguous_blocks = sdrec_sink_get_max_contiguous_write_blocks(writer);
    if ((max_contiguous_blocks == 0U) || (block_count > max_contiguous_blocks))
    {
        return SDREC_SINK_ERR_PARAM;
    }

    tick_ms = HAL_GetTick();
    build_state = writer->layout_state;

    memset(&writer->async, 0, sizeof(writer->async));
    writer->async.state = SDREC_SINK_ASYNC_DATA_BUSY;
    writer->async.data_lba = build_state.next_data_lba;
    writer->async.first_seq = build_state.write_seq;
    writer->async.block_count = block_count;
    writer->async.superblock_lba = sdrec_layout_current_superblock_lba(&writer->layout_cfg, &writer->layout_state);
    writer->async.tick_ms = tick_ms;

    for (i = 0U; i < block_count; i++)
    {
        if (((payloads[i] == NULL) && (payload_bytes[i] != 0U)) ||
            (payload_bytes[i] > SDREC_DATA_PAYLOAD_BYTES_V3))
        {
            writer->async.state = SDREC_SINK_ASYNC_IDLE;
            return SDREC_SINK_ERR_PARAM;
        }

        current_lba = build_state.next_data_lba;
        current_seq = build_state.write_seq;

        db = (sdrec_data_block_v3_t *)&writer->tx_burst_blocks[
            i * (SDREC_CARD_BLOCK_SIZE / sizeof(uint32_t))];
        sdrec_pack_data_block_v3(db,
                                    &build_state,
                                    current_lba,
                                    tick_ms,
                                    payloads[i],
                                    payload_bytes[i],
                                    writer->payload_flags);

        if (sdrec_layout_should_checkpoint(&writer->layout_cfg, &build_state) != 0)
        {
            writer->async.superblock_needed = 1U;
        }

        writer->async.last_data_lba = current_lba;
        writer->async.last_seq = current_seq;

        build_state.last_written_lba = current_lba;
        build_state.next_data_lba = sdrec_layout_wrap_lba(current_lba + 1U,
                                                          &writer->layout_cfg,
                                                          build_state.card_block_count);
        build_state.write_seq = current_seq + 1U;
    }

    if (writer->async.superblock_needed != 0U)
    {
        writer->async.checkpoint_state = writer->layout_state;
        writer->async.checkpoint_state.write_seq = writer->async.last_seq;
        writer->async.checkpoint_state.last_written_lba = writer->async.last_data_lba;
        writer->async.checkpoint_state.next_data_lba = build_state.next_data_lba;
    }

    writer->async.data_start_ms = HAL_GetTick();

    sd_status = sdrec_card_write_async_begin(writer->async.data_lba,
                                                writer->tx_burst_blocks,
                                                block_count);
    if ((sd_status != SDREC_CARD_IN_PROGRESS) && (sd_status != SDREC_CARD_OK))
    {
        writer->async.state = SDREC_SINK_ASYNC_IDLE;
        return SDREC_SINK_ERR_CARD_WRITE;
    }

    return SDREC_SINK_OK;
}

sdrec_sink_status_t sdrec_sink_begin_async_single(sdrec_sink_t *writer,
                                                           const void *payload,
                                                           uint32_t payload_bytes)
{
    const void *payload_list[1];
    uint32_t payload_size_list[1];

    payload_list[0] = payload;
    payload_size_list[0] = payload_bytes;

    return sdrec_sink_begin_async_batch(writer,
                                               payload_list,
                                               payload_size_list,
                                               1U);
}

sdrec_sink_status_t sdrec_sink_poll_async(sdrec_sink_t *writer,
                                                     sdrec_commit_report_t *step_info)
{
    sdrec_card_status_t sd_status;
    sdrec_superblock_v3_t *sb;

    if (writer == NULL)
    {
        return SDREC_SINK_ERR_PARAM;
    }

    if (writer->async.state == SDREC_SINK_ASYNC_IDLE)
    {
        return SDREC_SINK_OK;
    }

    sd_status = sdrec_card_write_async_poll();
    if (sd_status == SDREC_CARD_IN_PROGRESS)
    {
        return SDREC_SINK_IN_PROGRESS;
    }

    if (sd_status != SDREC_CARD_OK)
    {
        writer->async.state = SDREC_SINK_ASYNC_IDLE;
        writer->async.superblock_needed = 0U;
        return SDREC_SINK_ERR_CARD_WRITE;
    }

    if (writer->async.state == SDREC_SINK_ASYNC_DATA_BUSY)
    {
        writer->async.data_ms = HAL_GetTick() - writer->async.data_start_ms;

        if (writer->async.superblock_needed != 0U)
        {
            writer->async.state = SDREC_SINK_ASYNC_SUPERBLOCK_BUSY;

            sb = (sdrec_superblock_v3_t *)writer->tx_block;
            sdrec_pack_superblock_v3(sb,
                                        &writer->layout_cfg,
                                        &writer->async.checkpoint_state,
                                        writer->async.tick_ms);

            writer->async.superblock_start_ms = HAL_GetTick();
            sd_status = sdrec_card_write_async_begin(writer->async.superblock_lba,
                                                        writer->tx_block,
                                                        1U);
            if ((sd_status != SDREC_CARD_IN_PROGRESS) && (sd_status != SDREC_CARD_OK))
            {
                writer->async.state = SDREC_SINK_ASYNC_IDLE;
                writer->async.superblock_needed = 0U;
                return SDREC_SINK_ERR_CARD_WRITE;
            }

            return SDREC_SINK_IN_PROGRESS;
        }

        return sdrec_sink_finish_async_burst(writer,
                                                 step_info,
                                                 0);
    }

    if (writer->async.state == SDREC_SINK_ASYNC_SUPERBLOCK_BUSY)
    {
        writer->async.superblock_ms = HAL_GetTick() - writer->async.superblock_start_ms;

        return sdrec_sink_finish_async_burst(writer,
                                                 step_info,
                                                 1);
    }

    writer->async.state = SDREC_SINK_ASYNC_IDLE;
    writer->async.superblock_needed = 0U;
    return SDREC_SINK_ERR_STATE;
}

void sdrec_sink_fill_test_payload(void *payload,
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
