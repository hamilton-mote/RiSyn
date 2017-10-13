/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/**
 * @ingroup     drivers_push_button
 * @{
 *
 * @file
 * @brief       Driver for the PUSH BUTTON Sensor.
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 *
 * @}
 */

#include <string.h>

#include "push_button.h"
#include "xtimer.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

int16_t push_button_events;

void push_button_trigger(void* arg) {
    push_button_events++;
}

int push_button_init(push_button_t *dev, const push_button_params_t *params)
{    
    dev->p.gpio = params->gpio;
    push_button_events = 0;
    if (gpio_init_int(params->gpio, GPIO_IN_PU, GPIO_FALLING, push_button_trigger, NULL)) {
        return -1;
    }
    return 0;
}

int push_button_read(int16_t *button_events)
{
    *button_events = push_button_events;
    push_button_events = 0;
    return 0;
}

