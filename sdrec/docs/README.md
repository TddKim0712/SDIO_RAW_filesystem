# sdrec_library_clean

`sdrec` 는 **SD card raw recorder library** 라는 뜻으로 새로 묶은 이름이다.
기존 `raw_*` 묶음은 파일명, 타입명, 함수명이 너무 비슷해서
디버거 창에서 `app / stream / writer / source` 가 한꺼번에 보일 때
층이 잘 안 보였다. 이번 버전은 **역할 이름이 바로 보이게** 다시 나눴다.

핵심 목표는 3가지다.

1. **producer / pipe / sink / runtime 층 이름을 분명하게 분리**
2. **더미 동작을 pipe 바깥 source 모듈로 완전히 분리**
3. **나중에 SPI/UART DMA source 로 바꿔도 SDIO DMA consumer 는 그대로 재사용**

---

## 새 라이브러리 트리

- `include/`
  - `sdrec_build_config.h`
    - 빌드 타임 용량 설정만 둔다.
    - 예: slot 개수, async 최대 burst 개수
  - `sdrec_card_port.h`
    - SDIO/SD DMA 저수준 포트
    - 카드 init, read/write, async poll, retry policy
  - `sdrec_layout_v3.h`
    - on-card layout
    - superblock / data block / packet struct
  - `sdrec_sink.h`
    - payload block 을 SD card 로 내보내는 consumer
  - `sdrec_pipe.h`
    - source 와 sink 사이 packet FIFO
  - `sdrec_source_api.h`
    - source 인터페이스 공통층
  - `sdrec_source_dummy_cpu.h`
    - CPU 더미 source
  - `sdrec_source_wave_table.h`
    - const wave table source
  - `sdrec_source_wave_dma.h`
    - wave table 기반 DMA staging source 예시
  - `sdrec_runtime.h`
    - main 을 얇게 만드는 상위 orchestration 래퍼

- `source/`
  - 위 header 에 대응되는 구현

- `examples/`
  - `app_sdrec_profile.h`
  - `main_sdrec_example.c`

- `docs/`
  - `README.md`
  - `MIGRATION_GUIDE.md`
  - `OLD_TO_NEW_NAME_MAP.md`

---

## 이름 기준

이번 naming 은 계층이 바로 보이게 이렇게 맞췄다.

- `sdrec_card_*`
  - 하드웨어 SD card port 층
- `sdrec_layout_*`
  - 디스크 포맷 / layout 층
- `sdrec_sink_*`
  - SD 로 쓰는 consumer 층
- `sdrec_pipe_*`
  - source 와 sink 사이 FIFO / batch 층
- `sdrec_source_*`
  - producer 공통 인터페이스 층
- `sdrec_dummy_cpu_source_*`
  - CPU 더미 producer
- `sdrec_wave_table_source_*`
  - table producer
- `sdrec_wave_dma_source_*`
  - source DMA staging producer
- `sdrec_runtime_*`
  - app 진입점 wrapper

즉 이제 `begin / poll / reset` 같은 일반 동사라도
**prefix 만 보면 어느 층인지 바로 보이게** 했다.

---

## debugger 에서 바로 보이는 주요 이름

이전보다 눈에 띄도록 특히 아래를 바꿨다.

### runtime
- `runtime->pipe`
- `runtime->source_link`
- `runtime->cfg`

### pipe
- `pipe->sink`
- `pipe->ingress_slot_idx`
- `pipe->egress_slot_idx`
- `pipe->active_slot_idx_list`
- `pipe->active_slot_count`
- `pipe->queued_slot_count`
- `pipe->peak_queued_slot_count`
- `pipe->ingress_packet_count`
- `pipe->written_block_count`

즉 이제 디버거에서 봐도
**producer 입력 쪽 / queue 상태 / sink 진행 상태**가 한눈에 보인다.

---

## 더미 분리 원칙

`sdrec_pipe` 는 더 이상 더미를 만들지 않는다.

`sdrec_pipe` 의 책임은 오직:
- slot 관리
- ingress packet 적재
- READY batch 수집
- sink 로 drain

더미나 센서 흉내는 모두 source 쪽이다.

- CPU 더미: `sdrec_source_dummy_cpu.*`
- const wave table: `sdrec_source_wave_table.*`
- DMA staging: `sdrec_source_wave_dma.*`

이렇게 해두면,
나중에 SPI/UART DMA source 를 붙일 때
**source 모듈만 바꾸고 pipe / sink / card port 는 그대로 재사용**할 수 있다.

---

## source DMA 를 붙일 때 구조

목표 구조는 아래다.

1. source DMA 가 RAM staging buffer 를 채움
2. source 모듈이 packet 단위 READY 처리
3. `sdrec_pipe` 가 packet 을 slot 에 적재
4. `sdrec_sink` 가 READY slot batch 를 SDIO DMA 로 기록

즉
**source DMA 라인과 SDIO DMA 라인이 완전히 분리**된다.

현재 예시인 `sdrec_source_wave_dma.*` 는
실제 SPI/UART DMA 자리에 꽂기 쉬운 형태로 만든 틀이다.

---

## main 에 남겨야 하는 것

`main` 에는 가능하면 아래만 남긴다.

1. 보드 / CubeMX peripheral init
2. `sdrec_runtime_init_defaults()`
3. runtime cfg 채우기
4. source attach
5. loop 에서 `sdrec_runtime_poll()`

즉 raw recorder 내부 로직은 library 로 옮기고,
app 은 **정책 선택과 board init** 만 담당하게 하는 방향이다.

---

## 다음 단계 추천

1. 현재 프로젝트에 `include/`, `source/` 를 library 폴더로 추가
2. 기존 `Core/Src` 쪽 raw 관련 파일 참조 제거
3. `sdrec_runtime` 기반으로 main 얇게 교체
4. 처음에는 `sdrec_wave_dma_source` 로 구조 검증
5. 그 다음 실제 SPI/UART DMA callback 으로 교체

