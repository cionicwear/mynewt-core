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

#include "hal/hal_gpio.h"

#include <stdio.h>

#define HAL_GPIO_NUM_PINS 8

// Define the enum type outside the structure
enum gpio_dir {
    INPUT,
    OUTPUT
};

static struct {
    int val;
    enum gpio_dir dir;
} hal_gpio[HAL_GPIO_NUM_PINS];


int
hal_gpio_init_in(int pin, hal_gpio_pull_t pull)
{
    if (pin >= HAL_GPIO_NUM_PINS) {
        return -1;
    }
    hal_gpio[pin].dir = INPUT;
    switch (pull) {
    case HAL_GPIO_PULL_UP:
        hal_gpio[pin].val = 1;
        break;
    default:
        hal_gpio[pin].val = 0;
        break;
    }
    return 0;
}

int
hal_gpio_init_out(int pin, int val)
{
    if (pin >= HAL_GPIO_NUM_PINS) {
        return -1;
    }
    hal_gpio[pin].dir = OUTPUT;
    hal_gpio_write(pin,val);
    return 0;
}

void hal_gpio_write(int pin, int val)
{
    if (pin >= HAL_GPIO_NUM_PINS) {
        return;
    }
    if (hal_gpio[pin].dir != OUTPUT) {
        return;
    }
    hal_gpio[pin].val = (val != 0);
    // printf("hal_gpio set pin %2d to %1d\r", pin, hal_gpio[pin].val);
    fflush(stdout);
}

int
hal_gpio_read(int pin)
{
    if (pin >= HAL_GPIO_NUM_PINS) {
        return -1;
    }
    return hal_gpio[pin].val;
}

int
hal_gpio_toggle(int pin)
{
    int pin_state = (hal_gpio_read(pin) != 1);
    hal_gpio_write(pin, pin_state);
    return pin_state;
}

int
hal_gpio_irq_init(int pin, hal_gpio_irq_handler_t handler, void *arg,
                      hal_gpio_irq_trig_t trig, hal_gpio_pull_t pull)
{
    return hal_gpio_init_in(pin,pull);
}

void
hal_gpio_irq_enable(int pin)
{
    (void)pin;
}

void
hal_gpio_irq_release(int pin)
{
    (void)pin;
}

void
hal_gpio_irq_disable(int pin)
{
    (void)pin;
}

int hal_gpio_deinit(int pin)
{
    (void)pin;
    return 0;
}