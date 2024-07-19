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
#include "hal/hal_system.h"
#include "syscfg/syscfg.h"
#include "mcu/kinetis_common.h"

#if defined(FSL_FEATURE_SOC_RCM_COUNT) && (FSL_FEATURE_SOC_RCM_COUNT)
#include "fsl_rcm.h"
#elif defined(FSL_FEATURE_SOC_SMC_COUNT) && (FSL_FEATURE_SOC_SMC_COUNT > 1) /* MSMC */
#include "fsl_msmc.h"
#elif defined(FSL_FEATURE_SOC_ASMC_COUNT) && (FSL_FEATURE_SOC_ASMC_COUNT) /* ASMC */
#include "fsl_asmc.h"
#elif defined(FSL_FEATURE_SOC_CMC_COUNT) && (FSL_FEATURE_SOC_CMC_COUNT) /* CMC */
#include "fsl_cmc.h"
#endif

enum hal_reset_reason
hal_reset_cause(void)
{
    static enum hal_reset_reason reason;
    uint32_t reg;

    if (reason) {
        return reason;
    }
    reg = (uint32_t) SMC_GetPreviousResetSources(SMC0);

    if (reg & (kSMC_SourceWdog | kSMC_SourceLockup)) {
        reason = HAL_RESET_WATCHDOG;
    } else if (reg & kSMC_SourceSoftware) {
        reason = HAL_RESET_SOFT;
    } else if (reg & kSMC_SourcePin) {
        reason = HAL_RESET_PIN;
    } else if (reg & kSMC_SourcePor) {
        reason = HAL_RESET_POR;
    } else if (reg & kSMC_SourceWakeup) {
        reason = HAL_RESET_SYS_OFF_INT;
    } else if (reg & kSMC_SourceLvd) {
        reason = HAL_RESET_BROWNOUT;
    } else {
        reason = HAL_RESET_OTHER;
    }

    return reason;
}
