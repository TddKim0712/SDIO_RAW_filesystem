#ifndef SDREC_BUILD_CONFIG_H
#define SDREC_BUILD_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compile-time capacity knobs.
 * These are the only settings that still affect object size / static arrays.
 * Keep them in one place so the library can be moved as a standalone module.
 */
#ifndef SDREC_CARD_BLOCK_SIZE
#define SDREC_CARD_BLOCK_SIZE 512U
#endif

#ifndef SDREC_PIPE_SLOT_COUNT
#define SDREC_PIPE_SLOT_COUNT 4U
#endif

#ifndef SDREC_SINK_MAX_ASYNC_BLOCKS
#define SDREC_SINK_MAX_ASYNC_BLOCKS 8U
#endif

#if (SDREC_PIPE_SLOT_COUNT == 0U)
#error "SDREC_PIPE_SLOT_COUNT must be >= 1"
#endif

#if (SDREC_SINK_MAX_ASYNC_BLOCKS == 0U)
#error "SDREC_SINK_MAX_ASYNC_BLOCKS must be >= 1"
#endif

#ifndef SDREC_DUMMY_CPU_SOURCE_PERIOD_MS_DEFAULT
#define SDREC_DUMMY_CPU_SOURCE_PERIOD_MS_DEFAULT 1U
#endif

#ifndef SDREC_WAVE_TABLE_SOURCE_PERIOD_MS_DEFAULT
#define SDREC_WAVE_TABLE_SOURCE_PERIOD_MS_DEFAULT 1U
#endif

#ifndef SDREC_RUNTIME_CLOCK_DIV_DEFAULT
#define SDREC_RUNTIME_CLOCK_DIV_DEFAULT 5U
#endif

#ifndef SDREC_RUNTIME_LOG_EVERY_N_BLOCKS_DEFAULT
#define SDREC_RUNTIME_LOG_EVERY_N_BLOCKS_DEFAULT 64U
#endif

#ifndef SDREC_RUNTIME_BUS_WIDTH_DEFAULT
#define SDREC_RUNTIME_BUS_WIDTH_DEFAULT 0U /* SDREC_CARD_BUS_1BIT */
#endif

#ifdef __cplusplus
}
#endif

#endif /* SDREC_BUILD_CONFIG_H */
