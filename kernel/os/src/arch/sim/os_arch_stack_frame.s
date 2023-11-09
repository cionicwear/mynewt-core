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

#if defined MN_LINUX
#define CNAME(x)    x
#elif defined MN_OSX
#define CNAME(x)    _ ## x
#elif defined MN_FreeBSD
#define CNAME(x)    x
#else
#error "unsupported platform"
#endif

.section __TEXT,__text,regular,pure_instructions

.globl CNAME(os_arch_frame_init)
.globl _os_arch_frame_init
/*
 * void os_arch_frame_init(struct stack_frame *sf)
 */
CNAME(os_arch_frame_init):
    /* Function prologue for backtrace */
    sub sp, sp, #0x10
    str x29, [sp, #0x8]
    str x30, [sp, #0x10]
    mov x29, sp

    /*
     * At this point, we are executing on the main() stack:
     * ----------------
     * stack_frame ptr      [x29, #0]
     * ----------------
     * return address       [x30, #0]
     * ----------------
     * saved x29            [x29, #8]
     * ----------------
     * saved x30            [x29, #16]
     * ----------------
     */
    ldr x1, [x29, #0x10]     // Load 'sf' from saved x30
    mov x0, sp              // x0 = sp
    str x0, [x1, #0]        // sf->mainsp = sp

    /*
     * Switch the stack so the stack pointer stored in 'sf->sf_jb' points
     * to the task stack. This is slightly complicated because macOS wants
     * the incoming stack pointer to be 16-byte aligned.
     *
     * ----------------
     * sf (other fields)
     * ----------------
     * sf (sf_jb)           [x1, #0x4]
     * ----------------
     * sf (sf_mainsp)       [x1, #0]
     * ----------------
     * alignment padding    variable (0 to 12 bytes)
     * ----------------
     * savemask (0)         [x29, #0x4]
     * ----------------
     * pointer to sf_jb     [x29]
     * ----------------
     */
    mov x0, x29
    sub x0, x0, #0x8
    and x0, x0, #0xfffffffffffffff0 // Align x0 on a 16-byte boundary
    ldr x1, [x1, #0x4]       // Load &sf->sf_jb into x1
    str x0, [x29]            // Save the aligned x0 (new stack pointer)
    ldr x0, [x29, #0x10]     // Load &sf->sf_jb into x0
    mov x2, #0
   bl CNAME(sigsetjmp)      // sigsetjmp(sf->sf_jb, 0)
    cbz x0, 1f               // If x0 is 0, jump to 1
    ldr x0, [x29, #0x10]     // Load 'sf' from saved x30
    ldr x1, [x29, #0x8]      // Load saved x29
    ldr x2, [x29, #0x10]     // Load saved x30
    ldr x3, [x29]            // Load the aligned x0
    mov sp, x3               // Restore the stack pointer
    ldp x29, x30, [sp], #0x10
    ret                      // Return to os_arch_task_stack_init()

1:
    // Call os_arch_task_start(sf, rc)
    adrp x0, CNAME(os_arch_task_start)@page
    ldr x0, [x0, CNAME(os_arch_task_start)@pageoff]
    ldr x1, [x29, #0x10]     // Load 'sf' from saved x30
    ldr x2, [sp, #0x10]      // Load 'rc'
    blr x0

    // This function never returns

.section __TEXT,__text
