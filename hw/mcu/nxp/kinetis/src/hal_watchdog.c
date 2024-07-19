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

#include "hal/hal_watchdog.h"
#include "mcu/cmsis_nvic.h"

#include <assert.h>


#if defined(FSL_FEATURE_SOC_RCM_COUNT) && (FSL_FEATURE_SOC_RCM_COUNT)
#include "fsl_rcm.h"
#elif defined(FSL_FEATURE_SOC_SMC_COUNT) && (FSL_FEATURE_SOC_SMC_COUNT > 1) /* MSMC */
#include "fsl_msmc.h"
#elif defined(FSL_FEATURE_SOC_ASMC_COUNT) && (FSL_FEATURE_SOC_ASMC_COUNT) /* ASMC */
#include "fsl_asmc.h"
#elif defined(FSL_FEATURE_SOC_CMC_COUNT) && (FSL_FEATURE_SOC_CMC_COUNT) /* CMC */
#include "fsl_cmc.h"
#endif

#if MYNEWT_VAL(MCU_K32L3A6)
#include "fsl_wdog32.h"
#define WDOG_BASE   WDOG0
#define WDOG_GET_STATUS_FLAGS       WDOG32_GetStatusFlags
#define WDOG_CLEAR_STATUS_FLAGS     WDOG32_ClearStatusFlags
#define WDOG_GET_DFLT_CFG           WDOG32_GetDefaultConfig
#define WDOG_REFRESH                WDOG32_Refresh
#define WDOG_INIT                   WDOG32_Init
#define WDOG_ENABLE_IRQ             WDOG32_EnableInterrupts
#define WDOG_RUNNING_FLAG           kWDOG32_RunningFlag
#define WDOG_CLEAR_FLAG             kWDOG32_InterruptFlag
#define WDOG_IRQ_EN_FLAG            kWDOG32_InterruptEnable
#define WDOG_CFG_TYPE               wdog32_config_t
#define WDOG_IRQN                   WDOG0_IRQn
#else
#include "fsl_wdog.h"
#define WDOG_BASE   WDOG
#define WDOG_GET_STATUS_FLAGS       WDOG_GetStatusFlags
#define WDOG_CLEAR_STATUS_FLAGS     WDOG_ClearStatusFlags
#define WDOG_GET_DFLT_CFG           WDOG_GetDefaultConfig
#define WDOG_REFRESH                WDOG_Refresh
#define WDOG_INIT                   WDOG_Init
#define WDOG_ENABLE_IRQ             WDOG_EnableInterrupts
#define WDOG_RUNNING_FLAG           kWDOG_RunningFlag
#define WDOC_CLEAR_FLAG             kWDOG_TimeoutFlag
#define WDOG_IRQ_EN_FLAG            kWDOG_InterruptEnable
#define WDOG_CFG_TYPE               wdog_config_t
#define WDOG_IRQN                   WDOG_EWM_IRQn
#endif

#ifndef WATCHDOG_STUB
static WDOG_Type *wdog_base = WDOG_BASE;

static void nxp_hal_wdt_default_handler(void)
{
    assert(0);
}

/**@brief WDT interrupt handler. */
static void nxp_wdt_irq_handler(void)
{
    if (WDOG_GET_STATUS_FLAGS(wdog_base) && WDOG_RUNNING_FLAG) {
        WDOG_CLEAR_STATUS_FLAGS(wdog_base, WDOG_CLEAR_FLAG);
        nxp_hal_wdt_default_handler();
    }
}
#endif

int hal_watchdog_init(uint32_t expire_msecs)
{
#ifndef WATCHDOG_STUB
    WDOG_CFG_TYPE config;

    NVIC_SetVector(WDOG_IRQN, (uint32_t) nxp_wdt_irq_handler);
    WDOG_GET_DFLT_CFG(&config);
    config.timeoutValue = (expire_msecs * 32768ULL) / 1000;
    config.enableUpdate = true;
    WDOG_INIT(wdog_base, &config);
#endif

    return (0);
}

void hal_watchdog_enable(void)
{
#ifndef WATCHDOG_STUB
    WDOG_ENABLE_IRQ(wdog_base, WDOG_IRQ_EN_FLAG);
#endif
}

void hal_watchdog_tickle(void)
{
#ifndef WATCHDOG_STUB
    WDOG_REFRESH(wdog_base);
#endif
}
