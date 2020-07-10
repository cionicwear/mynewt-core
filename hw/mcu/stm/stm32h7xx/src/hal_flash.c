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
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal_flash.h"
#include "stm32h7xx_hal_flash_ex.h"
#include "hal/hal_flash_int.h"

static int stm32h7_flash_read(const struct hal_flash *dev, uint32_t address,
        void *dst, uint32_t num_bytes);
static int stm32h7_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes);
static int stm32h7_flash_erase_sector(const struct hal_flash *dev,
        uint32_t sector_address);
static int stm32h7_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz);
static int stm32h7_flash_init(const struct hal_flash *dev);

static const struct hal_flash_funcs stm32h7_flash_funcs = {
    .hff_read = stm32h7_flash_read,
    .hff_write = stm32h7_flash_write,
    .hff_erase_sector = stm32h7_flash_erase_sector,
    .hff_sector_info = stm32h7_flash_sector_info,
    .hff_init = stm32h7_flash_init
};

static const uint32_t stm32h7_flash_sectors[] = {
    0x08000000,     /* 128kB  */
    0x08020000,     /* 128kB  */
    0x08040000,     /* 128kB  */
    0x08060000,     /* 128kB  */
    0x08080000,     /* 128kB */
    0x080A0000,     /* 128kB */
    0x080C0000,     /* 128kB */
    0x080E0000,     /* 128kB */
    0x08100000,     /* 128kB */
    0x08120000,     /* 128kB */
    0x08140000,     /* 128kB */
    0x08160000,     /* 128kB */
    0x08180000,     /* 128kB */
    0x081A0000,     /* 128kB */
    0x081C0000,     /* 128kB */
    0x081E0000,     /* 128kB */
};

#define STM32H7_FLASH_NUM_AREAS                                         \
    (int)(sizeof(stm32h7_flash_sectors) /                               \
      sizeof(stm32h7_flash_sectors[0]))

const struct hal_flash stm32h7_flash_dev = {
    .hf_itf = &stm32h7_flash_funcs,
    .hf_base_addr = 0x08000000,
    .hf_size = 2 * 1024 * 1024,
    .hf_sector_cnt = STM32H7_FLASH_NUM_AREAS - 1,
    .hf_align = 1,
    .hf_erased_val = 0xff,
};

static int
stm32h7_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
        uint32_t num_bytes)
{
    memcpy(dst, (void *)address, num_bytes);
    return 0;
}

static int
stm32h7_flash_write(const struct hal_flash *dev, uint32_t address,
        const void *src, uint32_t num_bytes)
{
    uint32_t n_bytes = 0, rc;
	int32_t to_write = 0;
	uint8_t tmp[32];

    while(num_bytes)
	{
		n_bytes = address % 32;
		if(n_bytes)
		{
			// if data is not aligned on 256 bits we have to manually set 
			// the aligned address.
			address = address - n_bytes;
		}

		// reset the temporary buffer
		memset(tmp, 0xff, 32); 

		to_write = 32 - n_bytes;

		if(to_write > num_bytes)
			to_write = num_bytes;

		memcpy(tmp+n_bytes, src, to_write);

		num_bytes -= to_write;
		src += to_write;

		rc = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, address, (uint64_t)((uint32_t) tmp));

		if(rc)
			goto err;

		address += 0x20; // 256 bits
	}

err:
    return rc;
}

static void
stm32h7_flash_erase_sector_id(int sector_id, int bank)
{
    FLASH_Erase_Sector(sector_id, bank, FLASH_VOLTAGE_RANGE_1);
}

static int
stm32h7_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
{
    int i, bank = FLASH_BANK_1;

    for (i = 0; i < STM32H7_FLASH_NUM_AREAS - 1; i++) {
        if (stm32h7_flash_sectors[i] == sector_address) {
            if(i > 7){
                i -= 8;
                bank = FLASH_BANK_2;
            }
            stm32h7_flash_erase_sector_id(i, bank);
            return 0;
        }
    }

    return -1;
}

static int
stm32h7_flash_sector_info(const struct hal_flash *dev, int idx,
        uint32_t *address, uint32_t *sz)
{
    *address = stm32h7_flash_sectors[idx];
    *sz = stm32h7_flash_sectors[idx + 1] - stm32h7_flash_sectors[idx];
    return 0;
}

static int
stm32h7_flash_init(const struct hal_flash *dev)
{
    HAL_FLASH_Unlock();
    return 0;
}
