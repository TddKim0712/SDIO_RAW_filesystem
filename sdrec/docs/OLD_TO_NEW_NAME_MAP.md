# OLD TO NEW NAME MAP

## files

- `rawlog_options.h` -> `sdrec_build_config.h`
- `raw_crc32.*` -> `sdrec_crc32.*`
- `raw_log_core.*` -> `sdrec_layout_v3.*`
- `raw_diskio.*` -> `sdrec_card_port.*`
- `raw_log_writer.*` -> `sdrec_sink.*`
- `raw_log_stream.*` -> `sdrec_pipe.*`
- `raw_log_source.*` -> `sdrec_source_api.*`
- `raw_log_source_dummy.*` -> `sdrec_source_dummy_cpu.*`
- `raw_log_source_table.*` -> `sdrec_source_wave_table.*`
- `raw_log_source_table_data.*` -> `sdrec_source_wave_table_data.*`
- `raw_log_source_table_dma.*` -> `sdrec_source_wave_dma.*`
- `raw_log_app.*` -> `sdrec_runtime.*`

## top level types

- `raw_log_app_t` -> `sdrec_runtime_t`
- `raw_log_stream_t` -> `sdrec_pipe_t`
- `raw_log_writer_t` -> `sdrec_sink_t`
- `raw_sd_policy_t` -> `sdrec_card_policy_t`
- `raw_log_config_t` -> `sdrec_layout_cfg_t`

## source interface

- `raw_log_source_binding_t` -> `sdrec_source_link_t`
- `raw_log_source_ops_t` -> `sdrec_source_api_t`
- `raw_log_source_bind()` -> `sdrec_source_link_attach()`
- `raw_log_source_unbind()` -> `sdrec_source_link_detach()`
- `raw_log_source_service()` -> `sdrec_source_link_poll()`

## dummy / wave source

- `g_raw_log_source_dummy_ops` -> `g_sdrec_dummy_cpu_source_api`
- `g_raw_log_source_table_ops` -> `g_sdrec_wave_table_source_api`
- `g_raw_log_source_table_dma_ops` -> `g_sdrec_wave_dma_source_api`

## runtime

- `raw_log_app_begin()` -> `sdrec_runtime_start()`
- `raw_log_app_step()` -> `sdrec_runtime_poll()`

## pipe

- `raw_log_stream_begin()` -> `sdrec_pipe_open()`
- `raw_log_stream_acquire_write_ptr()` -> `sdrec_pipe_acquire_ingress_ptr()`
- `raw_log_stream_commit_write()` -> `sdrec_pipe_commit_ingress()`
- `raw_log_stream_sd_task()` -> `sdrec_pipe_drain_to_card()`

## sink

- `raw_log_writer_begin()` -> `sdrec_sink_open()`
- `raw_log_writer_service_async()` -> `sdrec_sink_poll_async()`
- `raw_log_writer_start_payloads_async()` -> `sdrec_sink_begin_async_batch()`
- `raw_log_writer_start_payload_async()` -> `sdrec_sink_begin_async_single()`

## card port async

- `raw_sd_write_blocks_async_start()` -> `sdrec_card_write_async_begin()`
- `raw_sd_write_blocks_async_service()` -> `sdrec_card_write_async_poll()`
- `raw_sd_write_blocks_async_busy()` -> `sdrec_card_write_async_busy()`

