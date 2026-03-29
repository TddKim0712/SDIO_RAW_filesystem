What changed
============
1. Add app_sd_test_config.h
   - Change only APP_SD_TEST_MODE between 1U and 4U.
   - Keep separate clock div defaults for 1-bit and 4-bit tests.

2. main.c now reads the mode/clock only from app_sd_test_config.h.
   - No source edit needed for each bus-width comparison.

3. raw_log_writer.c now clears step_info at the beginning of each write step.
   - Debugger values stay sane even when SD write fails early.

Quick use
=========
1. Start with:
   APP_SD_TEST_MODE         1U
   APP_SD_TEST_CLK_DIV_1BIT 2U

2. When 1-bit is stable, try:
   APP_SD_TEST_MODE         4U
   APP_SD_TEST_CLK_DIV_4BIT 8U

3. If 4-bit still fails, try APP_SD_TEST_CLK_DIV_4BIT = 10U or 46U.
