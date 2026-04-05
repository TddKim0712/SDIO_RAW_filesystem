#include "main.h"
#include "gpio.h"
#include "dma.h"
#include "usart.h"
#include "sdio.h"

#include "app_sdrec_profile.h"
#include "sdrec_runtime.h"
#include "sdrec_source_dummy_cpu.h"
#include "sdrec_source_wave_table.h"
#include "sdrec_source_wave_dma.h"
#include <stdio.h>

extern UART_HandleTypeDef huart2;
extern SD_HandleTypeDef hsd;

static sdrec_runtime_t g_sdrec_runtime;
static sdrec_dummy_cpu_source_t g_dummy_cpu_source;
static sdrec_wave_table_source_t g_wave_table_source;
static sdrec_wave_dma_source_t g_wave_dma_source;

void SystemClock_Config(void);

static void App_Init(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART2_UART_Init();
    MX_SDIO_SD_Init();
}

static void App_Fatal(const char *tag)
{
#if (APP_ENABLE_UART_LOG != 0U)
    printf("[FATAL] %s hal=0x%08lX sta=0x%08lX\r\n",
           tag,
           (unsigned long)sdrec_card_get_last_error(),
           (unsigned long)hsd.Instance->STA);
#else
    (void)tag;
#endif

    Error_Handler();
}

static void App_PrintCommitReport(const sdrec_commit_report_t *report)
{
#if (APP_ENABLE_UART_LOG != 0U)
    sdrec_card_retry_stats_t retry_stats;

    sdrec_card_get_retry_stats(&retry_stats);

    if ((report != NULL) &&
        ((report->seq % g_sdrec_runtime.pipe.sink.log_every_n_blocks) == 0U))
    {
        printf("[LOG] seq=%lu last_seq=%lu blocks=%lu data_lba=%lu total_ms=%lu max_ready=%lu drop=%lu wr_retry=%lu recover=%lu\r\n",
               (unsigned long)report->seq,
               (unsigned long)report->last_seq,
               (unsigned long)report->block_count,
               (unsigned long)report->data_lba,
               (unsigned long)report->total_ms,
               (unsigned long)g_sdrec_runtime.pipe.peak_queued_slot_count,
               (unsigned long)g_sdrec_runtime.pipe.dropped_packet_count,
               (unsigned long)retry_stats.write_retry_count,
               (unsigned long)retry_stats.recover_count);
    }
#else
    (void)report;
#endif
}

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1U, HAL_MAX_DELAY);
    return ch;
}

int main(void)
{
    sdrec_sink_status_t status;
    sdrec_commit_report_t report;
    uint8_t report_ready;

    App_Init();

    sdrec_runtime_init_defaults(&g_sdrec_runtime);
    AppSdrec_FillRuntimeCfg(&g_sdrec_runtime.cfg);

#if (APP_SDREC_USE_DUMMY_CPU_SOURCE != 0U)
    AppSdrec_ConfigDummy(&g_dummy_cpu_source);
    sdrec_runtime_attach_source(&g_sdrec_runtime,
                                &g_sdrec_dummy_cpu_source_api,
                                &g_dummy_cpu_source);
#elif (APP_SDREC_USE_WAVE_TABLE_SOURCE != 0U)
    AppSdrec_ConfigWaveTable(&g_wave_table_source);
    sdrec_runtime_attach_source(&g_sdrec_runtime,
                                &g_sdrec_wave_table_source_api,
                                &g_wave_table_source);
#elif (APP_SDREC_USE_WAVE_DMA_SOURCE != 0U)
    AppSdrec_ConfigWaveDma(&g_wave_dma_source);
    sdrec_runtime_attach_source(&g_sdrec_runtime,
                                &g_sdrec_wave_dma_source_api,
                                &g_wave_dma_source);
#else
    sdrec_runtime_detach_source(&g_sdrec_runtime);
#endif

    status = sdrec_runtime_start(&g_sdrec_runtime);
    if (status != SDREC_SINK_OK)
    {
        App_Fatal("sdrec_runtime_start");
    }

#if (APP_ENABLE_UART_LOG != 0U)
    printf("\r\n[BOOT] sdrec library example\r\n");
    printf("[CFG] bus=%lu clkdiv=%lu source=%s slots=%u\r\n",
           (unsigned long)g_sdrec_runtime.cfg.bus_width,
           (unsigned long)g_sdrec_runtime.cfg.transfer_clock_div,
           (g_sdrec_runtime.source_link.api != NULL) ? g_sdrec_runtime.source_link.api->name : "external",
           (unsigned int)SDREC_PIPE_SLOT_COUNT);
#endif

    for (;;)
    {
        status = sdrec_runtime_poll(&g_sdrec_runtime, &report, &report_ready);
        if ((status != SDREC_SINK_OK) && (status != SDREC_SINK_IN_PROGRESS))
        {
            App_Fatal("sdrec_runtime_poll");
        }

        if (report_ready != 0U)
        {
            App_PrintCommitReport(&report);
        }
    }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
