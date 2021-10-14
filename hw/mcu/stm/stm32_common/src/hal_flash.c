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

#include <string.h>
#include <os/mynewt.h>
#include <mcu/stm32_hal.h>
#include "hal/hal_flash_int.h"
#include "hal/hal_watchdog.h"

#define FLASH_IS_LINEAR    defined(FLASH_PAGE_SIZE)
#define FLASH_WRITE_SIZE   MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE)
#define FLASH_ERASED_VAL   MYNEWT_VAL(MCU_FLASH_ERASED_VAL)

#define STM32_FLASH_SIZE   (MYNEWT_VAL(STM32_FLASH_SIZE_KB) * 1024)

#if FLASH_IS_LINEAR
#ifdef EMULATED_SECTOR_SIZE
#define FLASH_SECTOR_SIZE  EMULATED_SECTOR_SIZE
#else
#define FLASH_SECTOR_SIZE  FLASH_PAGE_SIZE
#endif
#endif

static int stm32_flash_read(const struct hal_flash *dev, uint32_t address,
        void *dst, uint32_t num_bytes);
static int stm32_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes);
static int stm32_flash_erase_sector(const struct hal_flash *dev,
        uint32_t sector_address);
static int stm32_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz);
static int stm32_flash_init(const struct hal_flash *dev);

const struct hal_flash_funcs stm32_flash_funcs = {
    .hff_read = stm32_flash_read,
    .hff_write = stm32_flash_write,
    .hff_erase_sector = stm32_flash_erase_sector,
    .hff_sector_info = stm32_flash_sector_info,
    .hff_init = stm32_flash_init
};

#if !FLASH_IS_LINEAR
extern const uint32_t stm32_flash_sectors[];
#define FLASH_NUM_AREAS MYNEWT_VAL(STM32_FLASH_NUM_AREAS)
#else
#define FLASH_NUM_AREAS (STM32_FLASH_SIZE / FLASH_SECTOR_SIZE)
#endif

const struct hal_flash stm32_flash_dev = {
    .hf_itf = &stm32_flash_funcs,
    .hf_base_addr = FLASH_BASE,
    .hf_size = STM32_FLASH_SIZE,
    .hf_sector_cnt = FLASH_NUM_AREAS,
    .hf_align = FLASH_WRITE_SIZE,
    .hf_erased_val = FLASH_ERASED_VAL,
};

static int
stm32_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
        uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

#if FLASH_IS_LINEAR
static int
stm32_flash_write_linear(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    uint64_t val;
    uint32_t i;
    int rc;
    uint8_t align;
    uint32_t num_words;
    os_sr_t sr;

    align = dev->hf_align;

#if MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE) == 1
    num_words = num_bytes;
#elif MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE) == 2
    num_words = ((num_bytes - 1) >> 1) + 1;
#elif MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE) == 4
    num_words = ((num_bytes - 1) >> 2) + 1;
#elif MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE) == 8
    num_words = ((num_bytes - 1) >> 3) + 1;
#else
    #error "Unsupported MCU_FLASH_MIN_WRITE_SIZE"
#endif

    /* FIXME: L4 didn't have error clearing here, should check if other families
     *        need this (or can be removed)! */
    STM32_HAL_FLASH_CLEAR_ERRORS();

    for (i = 0; i < num_words; i++) {
        if (num_bytes < align) {
            memcpy(&val, &((uint8_t *)src)[i * align], num_bytes);
            memset((uint32_t *)&val + num_bytes, dev->hf_erased_val, align - num_bytes);
        } else {
            memcpy(&val, &((uint8_t *)src)[i * align], align);
        }

        /* FIXME: L1 was previously unlocking flash before erasing/programming,
         * and locking again afterwards. Maybe all MCUs should do the same?
         */
        OS_ENTER_CRITICAL(sr);
        rc = HAL_FLASH_Program(FLASH_PROGRAM_TYPE, address, val);
        OS_EXIT_CRITICAL(sr);
        if (rc != HAL_OK) {
            return rc;
        }

        address += align;

        /* underflowing is ok here... */
        num_bytes -= align;

        /*
         * Long writes take excessive time, and stall the idle thread,
         * so tickling the watchdog here to avoid reset...
         */
        if (MYNEWT_VAL(WATCHDOG_INTERVAL) > 0 && !(i % 32)) {
            hal_watchdog_tickle();
        }
    }

    return 0;
}
#endif

#if MYNEWT_VAL(MCU_STM32H7)
static int
stm32_flash_write_256_aligned(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    const uint8_t *sptr;
    uint32_t n_bytes = 0;
	int32_t left = 0, to_write = 0, rc;
	uint8_t tmp[32];
    os_sr_t sr;

    HAL_FLASH_Unlock();

    sptr = src;
	left = num_bytes;

	while(left)
	{
		n_bytes = address % 32;
		if(n_bytes)
		{
			// if data is not aligned on 256 bits we have to manually set 
			// the proper address.
			address = address - n_bytes;
		}

       
		// Make sure the word we're about to write is not already written
        // Writing twice to the same word location, would trig a ECC error
        // This would rise a Bus Fault each time the MCU tries to read this word
        memcpy(tmp, (uint8_t *)address, 32);
        for(uint8_t i = 0 ; i < 32 ; i++){
            if(tmp[i] != 0xff){
                rc = -1;
                break;
            }
        }

		to_write = 32 - n_bytes;

		if(to_write > left)
			to_write = left;

		memcpy(tmp+n_bytes, sptr, to_write);

		left -= to_write;
		sptr += to_write;

        OS_ENTER_CRITICAL(sr);
		rc = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, address, (uint64_t)((uint32_t) tmp));
        OS_EXIT_CRITICAL(sr);

		if(rc)
			break;

		address += 0x20; // 256 bits
	}

	HAL_FLASH_Lock();

    return rc;
}
#endif

#if !FLASH_IS_LINEAR
static int
stm32_flash_write_non_linear(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    const uint8_t *sptr;
    uint32_t i;
    int rc = 0;
    os_sr_t sr;
    
    /*
     * Clear status of previous operation.
     */
    STM32_HAL_FLASH_CLEAR_ERRORS();

#if MYNEWT_VAL(MCU_STM32H7)
    (void) sptr;
    (void) i;
    rc = stm32_flash_write_256_aligned(dev, address, src, num_bytes);
#else
    sptr = src;
    HAL_FLASH_Unlock();
    for (i = 0; i < num_bytes; i++) {
        OS_ENTER_CRITICAL(sr);
        rc = HAL_FLASH_Program(FLASH_PROGRAM_TYPE, address, sptr[i]);
        OS_EXIT_CRITICAL(sr);
        if (rc != 0) {
            HAL_FLASH_Lock();
            return rc;
        }

        address++;
    }
    HAL_FLASH_Lock();
#endif

    return rc;
}
#endif

static int
stm32_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    if (!num_bytes) {
        return -1;
    }

#if FLASH_IS_LINEAR
    return stm32_flash_write_linear(dev, address, src, num_bytes);
#else
    return stm32_flash_write_non_linear(dev, address, src, num_bytes);
#endif
}

#if !FLASH_IS_LINEAR
static int
stm32_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    FLASH_EraseInitTypeDef eraseinit;
    HAL_StatusTypeDef err;
    uint32_t SectorError;
    int i, rc = 0;
    os_sr_t sr;
    /*
     * Clear status of previous operation.
     */
    STM32_HAL_FLASH_CLEAR_ERRORS();

    HAL_FLASH_Unlock();
    for (i = 0; i < dev->hf_sector_cnt; i++) {
        if (stm32_flash_sectors[i] == sector_address) {
            eraseinit.TypeErase = FLASH_TYPEERASE_SECTORS;

#if defined (DUAL_BANK)
            if(IS_FLASH_PROGRAM_ADDRESS_BANK1(sector_address))
            {
                eraseinit.Banks = FLASH_BANK_1;
            }else
            {
                eraseinit.Banks = FLASH_BANK_2;   
                i = i - 8; 
            }
#endif
#ifdef FLASH_OPTCR_nDBANK
            eraseinit.Banks = FLASH_BANK_1; /* Only used for mass erase */
#endif
            eraseinit.Sector = i;
            eraseinit.NbSectors = 1;
            eraseinit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
            OS_ENTER_CRITICAL(sr);
            err = HAL_FLASHEx_Erase(&eraseinit, &SectorError);
            OS_EXIT_CRITICAL(sr);
            if (err) {
                rc = -1;
            }
            break;
        }
    }

    HAL_FLASH_Lock();
    return rc;
}

#else /* FLASH_IS_LINEAR */

int stm32_mcu_flash_erase_sector(const struct hal_flash *, uint32_t);

static int
stm32_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    return stm32_mcu_flash_erase_sector(dev, sector_address);
}

#endif

static int
stm32_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz)
{
    (void)dev;

#if FLASH_IS_LINEAR
    *address = dev->hf_base_addr + FLASH_SECTOR_SIZE * idx;
    *sz = FLASH_SECTOR_SIZE;
#else
    *address = stm32_flash_sectors[idx];
    *sz = stm32_flash_sectors[idx + 1] - stm32_flash_sectors[idx];
#endif

    return 0;
}

static int
stm32_flash_init(const struct hal_flash *dev)
{
    (void)dev;
    STM32_HAL_FLASH_INIT();
    return 0;
}
