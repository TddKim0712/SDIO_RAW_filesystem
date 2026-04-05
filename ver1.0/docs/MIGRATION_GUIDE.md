# MIGRATION GUIDE

## 1. 기존 호출 흐름에서 가장 먼저 바뀌는 것

예전에는 main 이 raw module 들을 직접 각각 불렀다.

```c
raw_log_stream_dummy_sensor_task(&g_raw_stream);
raw_log_stream_sd_task(&g_raw_stream, &step, &step_ready);
```

이제는 source 와 sink 를 runtime wrapper 가 묶는다.

```c
sdrec_runtime_poll(&g_sdrec_runtime, &report, &report_ready);
```

즉 main 입장에서는
- source
- pipe
- sink
를 따로 기억하지 않고,
보통은 runtime 하나만 본다.

---

## 2. include 경로 변경

기존:

```c
#include "raw_diskio.h"
#include "raw_log_stream.h"
#include "raw_log_source_dummy.h"
#include "raw_log_app.h"
```

신규:

```c
#include "sdrec_card_port.h"
#include "sdrec_pipe.h"
#include "sdrec_source_dummy_cpu.h"
#include "sdrec_runtime.h"
```

---

## 3. top-level API 이름 변경

- `raw_log_app_t` -> `sdrec_runtime_t`
- `raw_log_app_params_t` -> `sdrec_runtime_cfg_t`
- `raw_log_app_init_defaults()` -> `sdrec_runtime_init_defaults()`
- `raw_log_app_params_init_defaults()` -> `sdrec_runtime_cfg_init_defaults()`
- `raw_log_app_set_source()` -> `sdrec_runtime_attach_source()`
- `raw_log_app_clear_source()` -> `sdrec_runtime_detach_source()`
- `raw_log_app_begin()` -> `sdrec_runtime_start()`
- `raw_log_app_step()` -> `sdrec_runtime_poll()`

---

## 4. source attach 변경

기존:

```c
raw_log_app_set_source(&app, &g_raw_log_source_dummy_ops, &dummy_ctx);
```

신규:

```c
sdrec_runtime_attach_source(&runtime, &g_sdrec_dummy_cpu_source_api, &dummy_ctx);
```

wave table 과 DMA staging 도 같은 방식이다.

- `g_raw_log_source_table_ops` -> `g_sdrec_wave_table_source_api`
- `g_raw_log_source_table_dma_ops` -> `g_sdrec_wave_dma_source_api`

---

## 5. pipe 직접 사용 시 변경

기존:
- `raw_log_stream_begin()`
- `raw_log_stream_push_packet()`
- `raw_log_stream_acquire_write_ptr()`
- `raw_log_stream_commit_write()`
- `raw_log_stream_sd_task()`

신규:
- `sdrec_pipe_open()`
- `sdrec_pipe_push_packet()`
- `sdrec_pipe_acquire_ingress_ptr()`
- `sdrec_pipe_commit_ingress()`
- `sdrec_pipe_drain_to_card()`

즉 write ptr / commit 도
**ingress 방향임이 바로 보이게** 이름을 바꿨다.

---

## 6. sink 직접 사용 시 변경

기존:
- `raw_log_writer_begin()`
- `raw_log_writer_service_async()`
- `raw_log_writer_start_payloads_async()`

신규:
- `sdrec_sink_open()`
- `sdrec_sink_poll_async()`
- `sdrec_sink_begin_async_batch()`

---

## 7. card port async 호출 변경

기존:
- `raw_sd_write_blocks_async_start()`
- `raw_sd_write_blocks_async_service()`
- `raw_sd_write_blocks_async_busy()`

신규:
- `sdrec_card_write_async_begin()`
- `sdrec_card_write_async_poll()`
- `sdrec_card_write_async_busy()`

---

## 8. app 설정 구조 변경

기존에 전역 매크로나 `app_sd_test_config.h` 에 몰려 있던 값은
이제 runtime cfg 와 card policy struct 로 채운다.

```c
sdrec_runtime_t runtime;
sdrec_runtime_init_defaults(&runtime);

runtime.cfg.bus_width = SDREC_CARD_BUS_4BIT;
runtime.cfg.transfer_clock_div = 7U;
runtime.cfg.log_every_n_blocks = 64U;
runtime.cfg.payload_flags = SDREC_FLAG_PAYLOAD_TABLE_SENSOR;
runtime.cfg.sd_policy.write_retry_count = 2U;
runtime.cfg.layout_cfg.superblock_write_interval = 16U;
runtime.cfg.layout_cfg.stall_threshold_ms = 20U;
```

빌드 타임으로 남겨둔 것은
`sdrec_build_config.h` 안의 용량/배열 크기뿐이다.

---

## 9. 실제 프로젝트 반영 순서

1. `include/`, `source/`, `examples/` 를 새 library 폴더로 복사
2. 기존 raw 관련 파일을 build 대상에서 제외
3. include path 를 새 library/include 로 변경
4. main 에서 runtime 기반 호출로 교체
5. 처음에는 dummy CPU source 또는 wave table DMA source 로 검증
6. 그 다음 SPI/UART DMA callback 에 연결

