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

#include "mcu/stm32_hal.h"
#include "os/mynewt.h"
#include "hal/hal_system.h"
#include "stm32h7xx_hal.h"
#include "cionic/cionic.h"
#include "cionic/clog.h"
#include <time.h>

// #define RTC_CLOCK_SOURCE_HSE
#define RTC_CLOCK_SOURCE_LSE
// #define RTC_CLOCK_SOURCE_LSI 

#ifdef RTC_CLOCK_SOURCE_HSE
  #define RTC_ASYNCH_PREDIV       99U
  #define RTC_SYNCH_PREDIV        9U
  #define RCC_RTCCLKSOURCE_1MHZ   ((uint32_t)((uint32_t)RCC_BDCR_RTCSEL | (uint32_t)((HSE_VALUE/1000000U) << 12U)))
#else /* RTC_CLOCK_SOURCE_LSE || RTC_CLOCK_SOURCE_LSI */
  //use these prescalers to increment in seconds
  #define RTC_ASYNCH_PREDIV       127U
  #define RTC_SYNCH_PREDIV        255U
#endif /* RTC_CLOCK_SOURCE_HSE */

static RTC_HandleTypeDef        hRTC_Handle;

void
hal_system_reset(void)
{

#if MYNEWT_VAL(HAL_SYSTEM_RESET_CB)
    hal_system_reset_cb();
#endif

    while (1) {
        HAL_DEBUG_BREAK();
        NVIC_SystemReset();
    }
}

int
hal_debugger_connected(void)
{
#if (__CORTEX_M == 0)
    return 0;
#else
    return CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk;
#endif
}

uint32_t
HAL_GetTick(void)
{
    return os_time_get();
}

HAL_StatusTypeDef
HAL_InitTick (uint32_t TickPriority)
{
  return HAL_OK;
}

int hal_rtc_init(void)
{

  RCC_OscInitTypeDef        RCC_OscInitStruct;
  RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;

  /* Enable access to RTC domain */
  HAL_PWR_EnableBkUpAccess();

#ifdef RTC_CLOCK_SOURCE_LSE
  /* Configue LSE as RTC clock soucre */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
#elif defined (RTC_CLOCK_SOURCE_LSI)
  /* Configue LSI as RTC clock soucre */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
#elif defined (RTC_CLOCK_SOURCE_HSE)
  /* Configue HSE as RTC clock soucre */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  /* Ensure that RTC is clocked by 1MHz */
  PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_1MHZ;
#else
#error Please select the RTC Clock source
#endif /* RTC_CLOCK_SOURCE_LSE */

  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) == HAL_OK)
  {
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) == HAL_OK)
    {
      /* Enable RTC Clock */
      __HAL_RCC_RTC_ENABLE();
      /* The time base should be 1ms
         Time base = ((RTC_ASYNCH_PREDIV + 1) * (RTC_SYNCH_PREDIV + 1)) / RTC_CLOCK
         HSE as RTC clock
           Time base = ((99 + 1) * (9 + 1)) / 1MHz
                     = 1ms
         LSE as RTC clock
           Time base = ((31 + 1) * (0 + 1)) / 32.768KHz
                     = ~1ms
         LSI as RTC clock
           Time base = ((31 + 1) * (0 + 1)) / 32KHz
                     = 1ms
      */
      hRTC_Handle.Instance = RTC;
      hRTC_Handle.Init.HourFormat = RTC_HOURFORMAT_24;
      hRTC_Handle.Init.AsynchPrediv = RTC_ASYNCH_PREDIV;
      hRTC_Handle.Init.SynchPrediv = RTC_SYNCH_PREDIV;
      hRTC_Handle.Init.OutPut = RTC_OUTPUT_DISABLE;
      hRTC_Handle.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
      hRTC_Handle.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

      if(HAL_RTC_Init(&hRTC_Handle) != HAL_OK)
      {
        return HAL_ERROR;
      }
    }
  }
  return HAL_OK;
}

int hal_set_rtc_date_time(uint32_t epoch)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    struct tm *timeInfo;
    time_t rawtime = epoch;

    // Convert epoch time to calendar time
    timeInfo = gmtime(&rawtime);

    sTime.Hours = timeInfo->tm_hour;
    sTime.Minutes = timeInfo->tm_min;
    sTime.Seconds = timeInfo->tm_sec;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;

    if (HAL_RTC_SetTime(&hRTC_Handle, &sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
       return HAL_ERROR;
    }

    // Set the date
    sDate.WeekDay = timeInfo->tm_wday + 1;
    sDate.Month = timeInfo->tm_mon + 1;
    sDate.Date = timeInfo->tm_mday;
    sDate.Year = timeInfo->tm_year - 100;

    if (HAL_RTC_SetDate(&hRTC_Handle, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
       return HAL_ERROR;
    }

    CILOG(INFO, SYSTEM , "set RTC: %u-%u-%u:%u:%u:%u", sDate.Year, sDate.Month, sDate.Date, sTime.Hours, sTime.Minutes, sTime.Seconds);

    return HAL_OK;
}

uint32_t hat_get_rtc_epoch(void)
{
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    
    HAL_RTC_GetTime(&hRTC_Handle, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hRTC_Handle, &sDate, RTC_FORMAT_BIN);

    CILOG(INFO, SYSTEM , "%u-%u-%u:%u:%u:%u", sDate.Year, sDate.Month, sDate.Date, sTime.Hours, sTime.Minutes, sTime.Seconds);

    return HAL_OK;
}