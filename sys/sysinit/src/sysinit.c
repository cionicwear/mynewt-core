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

#include <stdio.h>
#include <stddef.h>
#include <limits.h>
#include "os/mynewt.h"

static void
sysinit_dflt_panic_cb(const char *file, int line, const char *func,
                      const char *expr, const char *msg)
{
#if MYNEWT_VAL(SYSINIT_PANIC_MESSAGE)
    if (msg != NULL) {
        fprintf(stderr, "sysinit failure: %s\n", msg);
    }
#endif

    __assert_func(file, line, func, expr);
}

sysinit_panic_fn *sysinit_panic_cb = sysinit_dflt_panic_cb;

uint8_t sysinit_active;

/**
 * Sets the sysinit panic function; i.e., the function which executes when
 * initialization fails.  By default, a panic triggers a failed assertion.
 */
void
sysinit_panic_set(sysinit_panic_fn *panic_cb)
{
    sysinit_panic_cb = panic_cb;
}

void
sysinit_start(void)
{
    sysinit_active = 1;
}

void
sysinit_end(void)
{
    sysinit_active = 0;
}

#if !SPLIT_LOADER && MYNEWT_VAL(UNITTEST)

void
sysinit_app(void)
{
    /*** Stage 0 */
    /* 0.0: os_pkg_init (kernel/os) */
    os_pkg_init();

    // /*** Stage 1 */
    // /* 1.0: tu_init (test/testutil) */
    // tu_init();

    // /*** Stage 2 */
    // /* 2.0: hal_bsp_init_trng (hw/bsp/native) */
    // hal_bsp_init_trng();

    // /*** Stage 9 */
    // /* 9.0: flash_map_init (sys/flash_map) */
    // flash_map_init();

    /*** Stage 50 */
    /* 50.0: config_pkg_init (sys/config) */
    config_pkg_init();

    // /*** Stage 100 */
    // /* 100.0: mfg_init (sys/mfg) */
    // mfg_init();
    // /* 100.1: modlog_init (sys/log/modlog) */
    // modlog_init();

    // /*** Stage 200 */
    // /* 200.0: native_sock_init (net/ip/native_sockets) */
    // native_sock_init();
    // /* 200.1: fatfs_pkg_init (fs/fatfs) */
    // fatfs_pkg_init();
    // /* 200.2: log_reboot_pkg_init (sys/reboot) */
    // log_reboot_pkg_init();

    /*** Stage 220 */
    /* 220.0: config_pkg_init_stage2 (sys/config) */
    config_pkg_init_stage2();

    // /*** Stage 500 */
    // /* 500.0: id_init (sys/id) */
    // id_init();
    // /* 500.1: imgmgr_module_init (mgmt/imgmgr) */
    // imgmgr_module_init();
    // /* 500.2: shell_init (sys/shell) */
    // shell_init();
    // /* 500.3: split_app_init (boot/split) */
    // split_app_init();

    // /*** Stage 501 */
    // /* 501.0: sensor_pkg_init (hw/sensor) */
    // sensor_pkg_init();
    // /* 501.1: img_mgmt_module_init (cmd/img_mgmt/port/mynewt) */
    // img_mgmt_module_init();
}

#endif
