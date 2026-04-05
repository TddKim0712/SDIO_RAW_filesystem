# Raw SDIO DMA 개선 요약

핵심 변경점:

1. **DMA 완료 플래그 race 수정 (`raw_diskio.c`)**
   - 기존 코드는 `HAL_SD_WriteBlocks_DMA()` 호출 **후** `irq_done/irq_error/start_ms`를 초기화했다.
   - DMA IRQ가 아주 빨리 들어오면 callback이 세운 완료/에러 플래그를 다시 0으로 덮어쓸 수 있다.
   - 수정본은 **DMA 시작 전에** 플래그와 시작 tick을 초기화한다.

2. **single-block async -> multi-block async burst (`raw_log_writer.*`)**
   - 새 API: `raw_log_writer_start_payloads_async()`
   - 여러 payload를 연속 data block으로 패킹해서 **한 번의 `HAL_SD_WriteBlocks_DMA(..., block_count)`** 로 전송한다.
   - `raw_log_writer_start_payload_async()` 는 새 API의 1-block wrapper 로 유지했다.
   - 카드 끝 wrap 직전에는 contiguous LBA 길이만큼만 burst 하도록 `raw_log_writer_get_max_contiguous_write_blocks()` 추가.

3. **stream inflight를 1슬롯 -> 여러 슬롯으로 확장 (`raw_log_stream.*`)**
   - READY 슬롯을 drain 순서로 최대 burst 크기까지 모아서 writer에 넘긴다.
   - 성공 시 inflight 슬롯 전부 release, 실패 시 전부 READY로 복구한다.
   - 현재 기본 burst 상한은 `min(RAW_LOG_STREAM_SLOT_COUNT, RAW_LOG_WRITER_ASYNC_MAX_BLOCKS)` 이다.

주의:

- 이 변경은 **throughput** 우선이다. superblock은 burst 안에서 interval을 딱 맞춰 끊기보다, burst 끝 기준으로 한 번 기록될 수 있다.
- 그래서 superblock 주기는 최대 `burst_count-1` block 만큼 늦어질 수 있다. 대신 recovery가 항상 burst tail 기준으로 맞춰진다.
- `prev_data_ms / prev_total_ms` 의미는 기존의 "per-block"보다 **실질적으로 burst 단위 timing**에 가까워진다.

추천:

- 먼저 **1bit + clkdiv 7 또는 5** 에서 이 수정본으로 CRC/seq gap/retry를 다시 보자.
- 그 다음 안정적이면 slot 수를 8로 늘리고 burst도 8까지 올리는 식으로 확장하는 게 좋다.
