/*
 * Copyright (C) 2018 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */


/**
 * @ingroup     drivers_isl29035
 * @{
 *
 * @file
 * @brief       Device driver implementation for the ISL29035 light sensor
 *
 * @author      Michael Andersen <m.andersen@berkeley.edu>
 *
 * @}
 */

#include "isl29035.h"
#include "isl29035-internal.h"
#include "periph/i2c.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

int isl29035_init(isl29035_t *dev, i2c_t i2c, uint8_t address,
                  isl29035_range_t range, isl29035_mode_t mode)
{
    int res;
    uint8_t tmp;

    /* initialize device descriptor */
    dev->i2c = i2c;
    dev->address = address;
    dev->mode = mode;
    dev->range = range;

    /* acquire exclusive access to the bus */
    i2c_acquire(dev->i2c);
    /* initialize the I2C bus */
    i2c_init_master(i2c, I2C_SPEED_NORMAL);

    /* configure and enable the sensor */
    tmp = (mode << 5);
    res = i2c_write_reg(dev->i2c, address, ISL29035_REG_CMD1, tmp);
    tmp = (ISL29035_RES_INT_16) | range;
    res += i2c_write_reg(dev->i2c, address, ISL29035_REG_CMD2, tmp);
    /* release the bus for other threads */
    i2c_release(dev->i2c);
    if (res < 2) {
        return -1;
    }
    return 0;
}

/**

This function returns the results of the previous light sensor reading
and starts the sampling of the next reading

*/
int isl29035_read(const isl29035_t *dev)
{
    uint8_t low, high;
    uint16_t res;
    int ret;
    uint8_t tmp;

    i2c_acquire(dev->i2c);
    /* read lighting value */
    ret = i2c_read_reg(dev->i2c, dev->address, ISL29035_REG_LDATA, &low);
    ret += i2c_read_reg(dev->i2c, dev->address, ISL29035_REG_HDATA, &high);

    tmp = dev->mode << 5;
    ret += i2c_write_reg(dev->i2c, dev->address, ISL29035_REG_CMD1, tmp);

    i2c_release(dev->i2c);
    if (ret < 3) {
        return -1;
    }
    res = (high << 8) | low;
    DEBUG("ISL29035: Raw value: %i - high: %i, low: %i\n", res, high, low);

    /* calculate and return the actual lux value */
    switch (dev->range) {
      case ISL29035_RANGE_1K:
        return (int) ((((uint32_t)res)*1000) >> 16);
      case ISL29035_RANGE_4K:
        return (int) ((((uint32_t)res)*4000) >> 16);
      case ISL29035_RANGE_16K:
        return (int) ((((uint32_t)res)*16000) >> 16);
      case ISL29035_RANGE_64K:
        return (int) ((((uint32_t)res)*64000) >> 16);
      default:
        return 0;
    }
}

int isl29035_enable(const isl29035_t *dev)
{
    int res;
    uint8_t tmp;

    i2c_acquire(dev->i2c);
    tmp = dev->mode;
    res = i2c_write_reg(dev->i2c, dev->address, ISL29035_REG_CMD1, tmp);
    if (res < 1) {
        i2c_release(dev->i2c);
        return -1;
    }
    i2c_release(dev->i2c);
    return 0;
}

int isl29035_disable(const isl29035_t *dev)
{
    int res;

    i2c_acquire(dev->i2c);
    res = i2c_write_reg(dev->i2c, dev->address, ISL29035_REG_CMD1, 0);
    if (res < 1) {
        i2c_release(dev->i2c);
        return -1;
    }
    i2c_release(dev->i2c);
    return 0;
}
