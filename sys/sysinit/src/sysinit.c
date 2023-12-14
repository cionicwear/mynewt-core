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

    /*** Stage 50 */
    /* 50.0: config_pkg_init (sys/config) */
    config_pkg_init();

    /*** Stage 220 */
    /* 220.0: config_pkg_init_stage2 (sys/config) */
    config_pkg_init_stage2();

}

#endif
