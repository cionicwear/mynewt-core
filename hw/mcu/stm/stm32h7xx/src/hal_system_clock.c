/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_rcc.h"
#include <assert.h>
#include <hal/hal_system.h>
#include <mcu/cortex_m7.h>

void
hal_system_clock_start(void)
{
    RCC_OscInitTypeDef osc;
    RCC_ClkInitTypeDef clk;
    RCC_PeriphCLKInitTypeDef periph;

    // Disable Instruction and Data cache for now, 
    // as it doesn't seem to be properly handled by mynewt. 
    SCB_DisableICache();
    SCB_DisableDCache();

    /*!< Supply configuration update enable */
    HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

    /* The voltage scaling allows optimizing the power consumption when the device is 
        clocked below the maximum system frequency, to update the voltage scaling value 
        regarding system frequency refer to product datasheet.  */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /*
     * CLK_IN       = HSE                   ... 25MHz
     * PLL_CLK_OUT  = CLK_IN / PLLM * PLLN  ... 480MHz
     * PLLCLK       = PLL_CLK_OUT / PLLP    ... SYSCLK
     * PLL48CLK     = PLL_CLK_OUT / PLLQ    ... USB clock
     * PLLDSICLK    = PLL_CLK_OUT / PLLR    ... DSI host
     */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState            = RCC_HSE_ON;
    osc.HSIState            = RCC_HSI_OFF;
    osc.CSIState            = RCC_CSI_OFF;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM            = 5;
    osc.PLL.PLLN            = 192;  
    osc.PLL.PLLFRACN        = 0;
    osc.PLL.PLLP            = 2;
    osc.PLL.PLLQ            = 2;
    osc.PLL.PLLR            = 2;

    osc.PLL.PLLVCOSEL       = RCC_PLL1VCOWIDE;
    osc.PLL.PLLRGE          = RCC_PLL1VCIRANGE_2;

    assert(HAL_OK == HAL_RCC_OscConfig(&osc));
  
/* Select PLL as system clock source and configure  bus clocks dividers */
    clk.ClockType           = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 | \
                                RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_D3PCLK1);
    clk.SYSCLKSource        = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider       = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider       = RCC_HCLK_DIV2;
    clk.APB3CLKDivider      = RCC_APB3_DIV2;  
    clk.APB1CLKDivider      = RCC_APB1_DIV2; 
    clk.APB2CLKDivider      = RCC_APB2_DIV2; 
    clk.APB4CLKDivider      = RCC_APB4_DIV2; 

    assert(HAL_OK == HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4));

    periph.PeriphClockSelection = RCC_PERIPHCLK_USART6 | RCC_PERIPHCLK_UART8 | RCC_PERIPHCLK_SDMMC;
    periph.PLL2.PLL2M = 2;
    periph.PLL2.PLL2N = 16;
    periph.PLL2.PLL2P = 1;
    periph.PLL2.PLL2Q = 1;
    periph.PLL2.PLL2R = 1;
    periph.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
    periph.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    periph.PLL2.PLL2FRACN = 0;
    periph.PLL3.PLL3M = 5;
    periph.PLL3.PLL3N = 192;
    periph.PLL3.PLL3P = 2;
    periph.PLL3.PLL3Q = 2;
    periph.PLL3.PLL3R = 2;
    periph.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
    periph.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    periph.PLL3.PLL3FRACN = 0;
    periph.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_PLL3;
    periph.Usart16ClockSelection = RCC_USART16CLKSOURCE_PLL3;
    periph.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL2;

    assert(HAL_OK == HAL_RCCEx_PeriphCLKConfig(&periph));
}
