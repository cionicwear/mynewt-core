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

#include <assert.h>
#include "os/mynewt.h"
#include <hal/hal_os_tick.h>

/*
 * XXX implement tickless mode for MCU other then STM32F1.
 */

/*
 * ST's MCUs seems to have problem with accessing AHB interface from SWD during SLEEP.
 * This makes it almost impossible to use with SEGGER SystemView, therefore when OS_SYSVIEW
 * is defined __WFI will become a loop waiting for pending interrupts.
 */
#if MYNEWT_VAL(OS_SYSVIEW)
#undef __WFI
#define __WFI() do { } while ((SCB->ICSR & (SCB_ICSR_ISRPENDING_Msk | SCB_ICSR_PENDSTSET_Msk)) == 0)
#else
/*
 * Errata for STM32F405, STM32F407, STM32F415, STM32F417.
 * When WFI instruction is placed at address like 0x080xxxx4
 * (also seen for addresses ending with xxx2). System may
 * crash.
 * __WFI function places WFI instruction at address ending with x0 or x8
 * for affected MCUs.
 */
#if defined(STM32F405xx) || defined(STM32F407xx) || \
    defined(STM32F415xx) || defined(STM32F417xx)
#undef __WFI
__attribute__((aligned(8), naked)) void static
__WFI(void)
{
     __ASM volatile("wfi\n"
                    "bx lr");
}
#endif
#endif

#if MYNEWT_VAL(STM32_CLOCK_LSE) == 0 || (((32768 / OS_TICKS_PER_SEC) * OS_TICKS_PER_SEC) != 32768) || !defined(STM32F1)

void
os_tick_idle(os_time_t ticks)
{
    OS_ASSERT_CRITICAL();
    __DSB();
    __WFI();
}

void
os_tick_init(uint32_t os_ticks_per_sec, int prio)
{
    uint32_t reload_val;

    reload_val = ((uint64_t)SystemCoreClock / os_ticks_per_sec) - 1;

    /* Set the system time ticker up */
    SysTick->LOAD = reload_val;
    SysTick->VAL = 0;
    SysTick->CTRL = 0x0007;

    /* Set the system tick priority */
    NVIC_SetPriority(SysTick_IRQn, prio);

    /*
     * Keep clocking debug even when CPU is sleeping, stopped or in standby.
     */
#if !MYNEWT_VAL(MCU_STM32H7)
#if !MYNEWT_VAL(MCU_STM32F0)
    DBGMCU->CR |= (DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);
#else
    DBGMCU->CR |= (DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);
#endif
#else
    DBGMCU->CR |= (DBGMCU_CR_DBG_SLEEPD1 | DBGMCU_CR_DBG_STOPD1 | DBGMCU_CR_DBG_STANDBYD1);
    DBGMCU->CR |= (DBGMCU_CR_DBG_SLEEPD2 | DBGMCU_CR_DBG_STOPD2 | DBGMCU_CR_DBG_STANDBYD2);
#endif
}

#endif
