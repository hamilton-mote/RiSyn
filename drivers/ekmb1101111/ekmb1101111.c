/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/**
 * @ingroup     drivers_ekmb1101111
 * @{
 *
 * @file
 * @brief       Driver for the EKMB1101111 PIR Motion Sensor.
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 *
 * @}
 */

#include <string.h>

#include "ekmb1101111.h"
#include "xtimer.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

bool     pin_high;
uint64_t pin_rise_time;
uint64_t accum_up_time;
uint64_t last_reset_time;

void ekmb1101111_trigger(void* arg) {
    ekmb1101111_t* dev = (ekmb1101111_t*) arg;
    int pin_now = gpio_read(dev->p.gpio);

    //We were busy counting
    if (pin_high) {
        //Add into accumulation
        uint64_t now = xtimer_usec_from_ticks64(xtimer_now64());
        accum_up_time += (now - pin_rise_time);
    }
    if (pin_now) { // Pin is rising
        pin_rise_time = xtimer_usec_from_ticks64(xtimer_now64());
        pin_high = true;
    } else {       // Pin is falling
        pin_high = false;
    }
}

int ekmb1101111_init(ekmb1101111_t *dev, const ekmb1101111_params_t *params) {
    dev->p.gpio = params->gpio;
    pin_high = false;
    accum_up_time = 0;
    pin_rise_time = 0;
    last_reset_time = xtimer_usec_from_ticks64(xtimer_now64());
    if (gpio_init_int(params->gpio, GPIO_IN_PD, GPIO_BOTH, ekmb1101111_trigger, dev)) {
        return -1;
    }
    return 0;
}

int ekmb1101111_read(ekmb1101111_t *dev, int16_t *occup) {
    uint64_t now = xtimer_usec_from_ticks64(xtimer_now64());
    *occup = (uint16_t)((accum_up_time * 10000) / (now - last_reset_time));
    last_reset_time = now;
    accum_up_time = 0;
    return 0;
}

