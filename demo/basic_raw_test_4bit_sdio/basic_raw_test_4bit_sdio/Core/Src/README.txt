main.c
  Clean STM32-style RAW V3 writer main.

raw_diskio.*
  Low-level SD polling access.

raw_crc32.*
  On-disk CRC32 helper.

raw_log_core.*
  RAW V3 layout, CRC, validation, state helpers.

raw_log_writer.*
  Recovery + continuous single-block writer.

sd_raw_csv_ad_nav_updated_range_erase_analysis.py
  Windows raw drive viewer + analyzer.
  Bottom-left parser pane is reduced to 60% width.
  Bottom-right analyzer adds:
    - LBA range analysis
    - tick-delta stall analysis
    - seq/LBA continuity check
    - boot_count change detection
    - V3 CRC checks
    - dummy test payload corruption check
    - anomaly list with double-click jump to LBA
