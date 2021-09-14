/*
 * <h2><center>&copy; COPYRIGHT 2016 STMicroelectronics</center></h2>
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "stm32h7xx_hal_pwr_ex.h"
#include "stm32h7xx_hal.h"
#include <assert.h>

/*
 * This allows an user to have a custom clock configuration by zeroing
 * every possible clock source in the syscfg.
 */
#if MYNEWT_VAL(STM32_CLOCK_HSE) || MYNEWT_VAL(STM32_CLOCK_LSE) || \
    MYNEWT_VAL(STM32_CLOCK_HSI) || MYNEWT_VAL(STM32_CLOCK_LSI)

/*
 * HSI is turned on by default, but can be turned off and use HSE instead.
 */
#if (((MYNEWT_VAL(STM32_CLOCK_HSE) != 0) + (MYNEWT_VAL(STM32_CLOCK_HSI) != 0)) < 1)
#error "At least one of HSE or HSI clock source must be enabled"
#endif

void
SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc_init;
    RCC_ClkInitTypeDef clk_init;
    HAL_StatusTypeDef status;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /* Enable the MCU instruction cache */
#if MYNEWT_VAL(STM32_ENABLE_ICACHE)
    SCB_EnableICache();
#endif

    /*!< Supply configuration update enable */
    HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

    /*
     * The voltage scaling allows optimizing the power consumption when the
     * device is clocked below the maximum system frequency, to update the
     * voltage scaling value regarding system frequency refer to product
     * datasheet.
     */
    __HAL_PWR_VOLTAGESCALING_CONFIG(MYNEWT_VAL(STM32_CLOCK_VOLTAGESCALING_CONFIG));

    osc_init.OscillatorType = RCC_OSCILLATORTYPE_NONE;
    /*
     * LSI is used to clock the independent watchdog and optionally the RTC.
     * It can be disabled per user request, but is automatically enabled again
     * when the IWDG is started.
     *
     * XXX currently the watchdog is not optional, so there's no point in
     * disabling LSI through syscfg.
     */
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_LSI;
#if MYNEWT_VAL(STM32_CLOCK_LSI)
    osc_init.LSIState = RCC_LSI_ON;
#else
    osc_init.LSIState = RCC_LSI_OFF;
#endif

    /*
     * LSE is only used to clock the RTC.
     */
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_LSE;
#if (MYNEWT_VAL(STM32_CLOCK_LSE) == 0)
    osc_init.LSEState = RCC_LSE_OFF;
#elif MYNEWT_VAL(STM32_CLOCK_LSE_BYPASS)
    osc_init.LSEState = RCC_LSE_BYPASS;
#else
    osc_init.LSEState = RCC_LSE_ON;
#endif

    /*
     * HSE Oscillator (can be used as PLL, SYSCLK and RTC clock source)
     */
#if MYNEWT_VAL(STM32_CLOCK_HSE)
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSE;
#if MYNEWT_VAL(STM32_CLOCK_HSE_BYPASS)
    osc_init.HSEState = RCC_HSE_BYPASS;
#else
    osc_init.HSEState = RCC_HSE_ON;
#endif
#endif

    /*
     * HSI Oscillator (can be used as PLL and SYSCLK clock source). It is
     * already turned on by default but a new calibration setting might be
     * used. If the user chooses to turn it off, it must be turned off after
     * SYSCLK was updated to use HSE/PLL.
     */
#if MYNEWT_VAL(STM32_CLOCK_HSI)
    osc_init.OscillatorType |= RCC_OSCILLATORTYPE_HSI;
    osc_init.HSIState = RCC_HSI_ON;
    /* HSI calibration is not optional when HSI is enabled */
    osc_init.HSICalibrationValue = MYNEWT_VAL(STM32_CLOCK_HSI_CALIBRATION);

#if MYNEWT_VAL(STM32_CLOCK_HSI) && \
        !IS_RCC_CALIBRATION_VALUE(MYNEWT_VAL(STM32_CLOCK_HSI_CALIBRATION))
#error "Invalid HSI calibration value"
#endif
#endif

    /*
     * Default to HSE as source, when both HSE and HSI are enabled.
     *
     * TODO: option to let the PLL turned off could be added, because
     * both HSI and HSE can be used as SYSCLK source.
     */
    osc_init.PLL.PLLState = RCC_PLL_ON;
#if MYNEWT_VAL(STM32_CLOCK_HSE)
    osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;
#else
    osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSI;
#endif

#if !IS_RCC_PLLM_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLM))
#error "PLLM value is invalid"
#endif

#if !IS_RCC_PLLN_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLN))
#error "PLLN value is invalid"
#endif

/*
 * Redefine this macro for F7, because the Cube HAL is using typecast which
 * is not valid when trying to catch errors at build time.
 */
#if MYNEWT_VAL(MCU_STM32F7)
#undef IS_RCC_PLLP_VALUE
#define IS_RCC_PLLP_VALUE(VALUE) (((VALUE) == 2U) || ((VALUE) == 4U) || \
                                  ((VALUE) == 6U) || ((VALUE) == 8U))
#endif

#if !IS_RCC_PLLP_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLP))
#error "PLLP value is invalid"
#endif

#if !IS_RCC_PLLQ_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLQ))
#error "PLLQ value is invalid"
#endif

    osc_init.PLL.PLLM = MYNEWT_VAL(STM32_CLOCK_PLL_PLLM);
    osc_init.PLL.PLLN = MYNEWT_VAL(STM32_CLOCK_PLL_PLLN);
    osc_init.PLL.PLLP = MYNEWT_VAL(STM32_CLOCK_PLL_PLLP);
    osc_init.PLL.PLLQ = MYNEWT_VAL(STM32_CLOCK_PLL_PLLQ);

#if MYNEWT_VAL(STM32_CLOCK_PLL_PLLR)

#if !IS_RCC_PLLR_VALUE(MYNEWT_VAL(STM32_CLOCK_PLL_PLLR))
#error "PLLR value is invalid"
#endif

    osc_init.PLL.PLLR = MYNEWT_VAL(STM32_CLOCK_PLL_PLLR);

#endif /* MYNEWT_VAL(STM32_CLOCK_PLL_PLLR) */

    osc_init.PLL.PLLVCOSEL       = RCC_PLL1VCOWIDE;
    osc_init.PLL.PLLRGE          = RCC_PLL1VCIRANGE_2;

    status = HAL_RCC_OscConfig(&osc_init);
    if (status != HAL_OK) {
        assert(0);
    }

#if MYNEWT_VAL(STM32_CLOCK_ENABLE_OVERDRIVE)
    /*
     * Activate the Over-Drive mode
     */
    status = HAL_PWREx_EnableOverDrive();
    if (status != HAL_OK) {
        assert(0);
    }
#endif

    /*
     * Select PLL as system clock source and configure the HCLK, PCLK1 and
     * PCLK2 clocks dividers. HSI and HSE are also valid system clock sources,
     * although there is no much point in supporting them now.
     */
    clk_init.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
    clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;

// #if !IS_RCC_HCLK(MYNEWT_VAL(STM32_CLOCK_AHB_DIVIDER))
// #error "AHB clock divider is invalid"
// #endif

// #if !IS_RCC_PCLK(MYNEWT_VAL(STM32_CLOCK_APB1_DIVIDER))
// #error "APB1 clock divider is invalid"
// #endif

// #if !IS_RCC_PCLK(MYNEWT_VAL(STM32_CLOCK_APB2_DIVIDER))
// #error "APB2 clock divider is invalid"
// #endif
    clk_init.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clk_init.AHBCLKDivider = MYNEWT_VAL(STM32_CLOCK_AHB_DIVIDER);
    clk_init.APB1CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB1_DIVIDER);
    clk_init.APB2CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB2_DIVIDER);
    clk_init.APB3CLKDivider = MYNEWT_VAL(STM32_CLOCK_APB3_DIVIDER);

#if !IS_FLASH_LATENCY(MYNEWT_VAL(STM32_FLASH_LATENCY))
#error "Flash latency value is invalid"
#endif

    status = HAL_RCC_ClockConfig(&clk_init, MYNEWT_VAL(STM32_FLASH_LATENCY));
    if (status != HAL_OK) {
        assert(0);
    }

#if MYNEWT_VAL(STM32_CLOCK_PLL2)
    /* PLL2 for USART interface */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_UART4
                              |RCC_PERIPHCLK_USART6|RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_USART3
                              |RCC_PERIPHCLK_UART5|RCC_PERIPHCLK_UART7
                              |RCC_PERIPHCLK_UART8 | RCC_PERIPHCLK_ADC;
    PeriphClkInitStruct.PLL2.PLL2M = MYNEWT_VAL(STM32_CLOCK_PLL2_PLLM);
    PeriphClkInitStruct.PLL2.PLL2N = MYNEWT_VAL(STM32_CLOCK_PLL2_PLLN);
    PeriphClkInitStruct.PLL2.PLL2P = MYNEWT_VAL(STM32_CLOCK_PLL2_PLLP);
    PeriphClkInitStruct.PLL2.PLL2Q = MYNEWT_VAL(STM32_CLOCK_PLL2_PLLQ);
    PeriphClkInitStruct.PLL2.PLL2R = MYNEWT_VAL(STM32_CLOCK_PLL2_PLLR);
    PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
    PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
    PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_PLL2;
    PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_PLL2;
    PeriphClkInitStruct.FmcClockSelection = RCC_FMCCLKSOURCE_PLL2;
#endif

#if MYNEWT_VAL(STM32_CLOCK_PLL3)
    /* PLL2 for USART interface */
    PeriphClkInitStruct.PeriphClockSelection |= RCC_PERIPHCLK_SPI123;
    PeriphClkInitStruct.PLL3.PLL3M = MYNEWT_VAL(STM32_CLOCK_PLL3_PLLM);
    PeriphClkInitStruct.PLL3.PLL3N = MYNEWT_VAL(STM32_CLOCK_PLL3_PLLN);
    PeriphClkInitStruct.PLL3.PLL3P = MYNEWT_VAL(STM32_CLOCK_PLL3_PLLP);
    PeriphClkInitStruct.PLL3.PLL3Q = MYNEWT_VAL(STM32_CLOCK_PLL3_PLLQ);
    PeriphClkInitStruct.PLL3.PLL3R = MYNEWT_VAL(STM32_CLOCK_PLL3_PLLR);
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
    PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL3;
#endif

#if MYNEWT_VAL(STM32_SDMMC_CLOCK_SEL)
    PeriphClkInitStruct.PeriphClockSelection |= RCC_PERIPHCLK_SDMMC;
    PeriphClkInitStruct.SdmmcClockSelection = MYNEWT_VAL(STM32_SDMMC_CLOCK_SEL);
#endif

#if MYNEWT_VAL(STM32_FMC_CLOCK_SEL)
    PeriphClkInitStruct.PeriphClockSelection |= RCC_PERIPHCLK_FMC;
    PeriphClkInitStruct.FmcClockSelection = MYNEWT_VAL(STM32_FMC_CLOCK_SEL);
#endif

#if MYNEWT_VAL(STM32_USB_CLOCK_SEL)
    PeriphClkInitStruct.PeriphClockSelection |= RCC_PERIPHCLK_USB;
    PeriphClkInitStruct.UsbClockSelection = MYNEWT_VAL(STM32_USB_CLOCK_SEL);
#endif
 // Enable RNG
PeriphClkInitStruct.PeriphClockSelection |= RCC_PERIPHCLK_RNG;
PeriphClkInitStruct.RngClockSelection = RCC_RNGCLKSOURCE_PLL;

#if MYNEWT_VAL(STM32_CLOCK_PLL3) || MYNEWT_VAL(STM32_CLOCK_PLL2)
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        assert(0);
    }
#endif
    /*
     * These are flash instruction and data caches, not the be confused with
     * MCU caches.
     */
#if INSTRUCTION_CACHE_ENABLE
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
#endif

#if DATA_CACHE_ENABLE
    __HAL_FLASH_DATA_CACHE_ENABLE();
#endif

      /*activate CSI clock mondatory for I/O Compensation Cell*/
  __HAL_RCC_CSI_ENABLE() ;

  /* Enable SYSCFG clock mondatory for I/O Compensation Cell */
  __HAL_RCC_SYSCFG_CLK_ENABLE() ;

  /* Enables the I/O Compensation Cell */
  HAL_EnableCompensationCell();
}
#endif
