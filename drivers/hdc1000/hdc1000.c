/*
 * Copyright (C) 2014 PHYTEC Messtechnik GmbH
 *               2017 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/**
 * @ingroup     drivers_hdc1000
 * @{
 *
 * @file
 * @brief       Driver for the TI HDC1000 Humidity and Temperature Sensor.
 *
 * @author      Johann Fischer <j.fischer@phytec.de>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <string.h>

#include "assert.h"
#include "xtimer.h"
#include "periph/i2c.h"
#include "hdc1000.h"
#include "xtimer.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define I2C_SPEED                  I2C_SPEED_FAST

#ifndef HDC1000_RENEW_INTERVAL
#define HDC1000_RENEW_INTERVAL 5000000ul
#endif

int16_t temp_cached = 0;
int16_t hum_cached = 0;

void hdc1000_renew_timer_cb(void* arg) {
    temp_cached = 0;
    hum_cached = 0;
}

xtimer_t hdc1000_renew_timer = {.callback = hdc1000_renew_timer_cb};

int hdc1000_init(hdc1000_t *dev, const hdc1000_params_t *params)
{
    uint8_t reg[2];
    uint16_t tmp;

    /* write device descriptor */
    memcpy(&dev->p, params, sizeof(hdc1000_params_t));

    /* initialize the I2C bus */
    i2c_acquire(dev->p.i2c);
    if (i2c_init_master(dev->p.i2c, I2C_SPEED) < 0) {
        i2c_release(dev->p.i2c);
        return HDC1000_NOBUS;
    }

    /* try if we can interact with the device by reading its manufacturer ID */
    if (i2c_read_regs(dev->p.i2c, dev->p.addr,
                      HDC1000_MANUFACTURER_ID, reg, 2) != 2) {
        i2c_release(dev->p.i2c);
        return HDC1000_NOBUS;
    }

    tmp = ((uint16_t)reg[0] << 8) | reg[1];
    if (tmp != HDC1000_MID_VALUE) {
        i2c_release(dev->p.i2c);
        return HDC1000_NODEV;
    }

    /* set resolution for both sensors and sequence mode */
    tmp = (HDC1000_SEQ_MOD | dev->p.res);
    reg[0] = (tmp >> 8);
    reg[1] = tmp;

    if (i2c_write_regs(dev->p.i2c, dev->p.addr, HDC1000_CONFIG, reg, 2) != 2) {
        i2c_release(dev->p.i2c);
        return HDC1000_NOBUS;
    }
    i2c_release(dev->p.i2c);

    /* all set */
    return HDC1000_OK;
}

int hdc1000_trigger_conversion(const hdc1000_t *dev)
{
    int status = HDC1000_OK;
    assert(dev);

    i2c_acquire(dev->p.i2c);

    /* Trigger the measurements by executing a write access
     * to the address 0x00 (HDC1000_TEMPERATURE).
     * Conversion Time is 6.50ms for each value for 14 bit resolution.
     */
    if (i2c_write_byte(dev->p.i2c, dev->p.addr, HDC1000_TEMPERATURE) != 1) {
        status = HDC1000_BUSERR;
    }

    i2c_release(dev->p.i2c);
    return status;
}

int hdc1000_get_results(const hdc1000_t *dev, int16_t *temp, int16_t *hum)
{
    int status = HDC1000_OK;
    assert(dev);

    uint8_t buf[4];

    /* first we read the RAW results from the device */
    i2c_acquire(dev->p.i2c);
    if (i2c_read_bytes(dev->p.i2c, dev->p.addr, buf, 4) != 4) {
        status = HDC1000_BUSERR;
    }
    i2c_release(dev->p.i2c);

    if (status == HDC1000_OK) {
        /* if all ok, we convert the values to their physical representation */
        uint16_t traw = ((uint16_t)buf[0] << 8) | buf[1];
        temp_cached = (int16_t)((((int32_t)traw * 16500) >> 16) - 4000);

        uint16_t hraw = ((uint16_t)buf[2] << 8) | buf[3];
        hum_cached  = (int16_t)(((int32_t)hraw * 10000) >> 16);

        if (temp) {
            *temp = temp_cached;
            temp_cached = 0;
        }
        if (hum) {
            *hum = hum_cached;
            hum_cached = 0;
        }
        if ((hum_cached != 0) || (temp_cached != 0)) {
            xtimer_set(&hdc1000_renew_timer, HDC1000_RENEW_INTERVAL);
        }
    }

    return status;
}

int hdc1000_read(const hdc1000_t *dev, int16_t *temp, int16_t *hum)
{
    /* Return cached data to mitigate redundant sensing */
    if ((temp == NULL) && (hum != NULL) && (hum_cached != 0)) {
        *hum = hum_cached;
        hum_cached = 0;
        xtimer_remove(&hdc1000_renew_timer);
        return HDC1000_OK;
    } else if ((temp != NULL) && (hum == NULL) && (temp_cached != 0)) {
        *temp = temp_cached;
        temp_cached = 0;
        xtimer_remove(&hdc1000_renew_timer);
        return HDC1000_OK;
    }

    if (hdc1000_trigger_conversion(dev) != HDC1000_OK) {
        return HDC1000_BUSERR;
    }
    xtimer_usleep(HDC1000_CONVERSION_TIME);
    return hdc1000_get_results(dev, temp, hum);
}
