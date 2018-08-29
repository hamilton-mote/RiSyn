/*
 * Copyright (C) 2018 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_lsm303c
 * @{
 *
 * @file
 * @brief       Device driver implementation for the LSM303C 3D accelerometer/magnetometer.
 *
 * @author      Michael Andersen <m.andersen@cs.berkeley.edu>
 *
 * @}
 */

#include "lsm303c.h"
#include "lsm303c-internal.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

int lsm303c_init(lsm303c_t *dev, i2c_t i2c,
                    uint8_t acc_address,
                    lsm303c_acc_sample_rate_t acc_sample_rate,
                    lsm303c_acc_scale_t acc_scale,
                    uint8_t mag_address,
                    lsm303c_mag_sample_rate_t mag_sample_rate)
{
    int res;
    uint8_t tmp;

    dev->i2c = i2c;
    dev->acc_address = acc_address;
    dev->mag_address = mag_address;
    dev->acc_scale   = acc_scale;

    /* Acquire exclusive access to the bus. */
    i2c_acquire(dev->i2c);
    i2c_init_master(i2c, I2C_SPEED_NORMAL);

    DEBUG("lsm303c reboot...");
    res = i2c_write_reg(dev->i2c, dev->acc_address,
                        LSM303C_REG_CTRL5_A, 0x40);
    /* Release the bus for other threads. */
    i2c_release(dev->i2c);
    DEBUG("[OK]\n");

    /* configure accelerometer */
    /* enable all three axis and set sample rate */
    tmp = (LSM303C_CTRL1_A_XEN
          | LSM303C_CTRL1_A_YEN
          | LSM303C_CTRL1_A_ZEN
          | acc_sample_rate);
    i2c_acquire(dev->i2c);
    res += i2c_write_reg(dev->i2c, dev->acc_address,
                         LSM303C_REG_CTRL1_A, tmp);
    /* update on read, MSB @ low address, scale and high-resolution */
    tmp = (acc_scale);
    res += i2c_write_reg(dev->i2c, dev->acc_address,
                         LSM303C_REG_CTRL4_A, tmp);

    /* configure magnetometer  */
    res += i2c_write_reg(dev->i2c, dev->mag_address,
                         LSM303C_REG_CTRL3_M, 0x0c);
                         res += i2c_write_reg(dev->i2c, dev->mag_address,
                                              LSM303C_REG_CTRL3_M, 0x60);
    /*  set sample rate */
    tmp = mag_sample_rate;
    res += i2c_write_reg(dev->i2c, dev->mag_address,
                         LSM303C_REG_CTRL1_M, tmp);
    /* set continuous mode */
    tmp = LSM303C_CTRL3_M_SINGLE_CONV;
    res += i2c_write_reg(dev->i2c, dev->mag_address,
                         LSM303C_REG_CTRL3_M, tmp);

                         res += i2c_write_reg(dev->i2c, dev->mag_address, LSM303C_REG_CTRL5_M, 0x40);
    i2c_release(dev->i2c);
    return (res < 5) ? -1 : 0;
}

int lsm303c_read_acc(const lsm303c_t *dev, lsm303c_3d_data_t *data)
{
    int res;
    uint8_t tmp;

    i2c_acquire(dev->i2c);
    i2c_read_reg(dev->i2c, dev->acc_address, LSM303C_REG_STATUS_A, &tmp);
    DEBUG("lsm303c status: %x\n", tmp);
    DEBUG("lsm303c: wait for acc values ... ");

    res = i2c_read_reg(dev->i2c, dev->acc_address,
                       LSM303C_REG_OUT_X_L_A, &tmp);
    data->x_axis = tmp;
    res += i2c_read_reg(dev->i2c, dev->acc_address,
                        LSM303C_REG_OUT_X_H_A, &tmp);
    data->x_axis |= tmp<<8;
    res += i2c_read_reg(dev->i2c, dev->acc_address,
                       LSM303C_REG_OUT_Y_L_A, &tmp);
    data->y_axis = tmp;
    res += i2c_read_reg(dev->i2c, dev->acc_address,
                        LSM303C_REG_OUT_Y_H_A, &tmp);
    data->y_axis |= tmp<<8;
    res += i2c_read_reg(dev->i2c, dev->acc_address,
                       LSM303C_REG_OUT_Z_L_A, &tmp);
    data->z_axis = tmp;
    res += i2c_read_reg(dev->i2c, dev->acc_address,
                        LSM303C_REG_OUT_Z_H_A, &tmp);
    data->z_axis |= tmp<<8;
    i2c_release(dev->i2c);
    DEBUG("read ... ");

    data->x_axis = data->x_axis>>4;
    data->y_axis = data->y_axis>>4;
    data->z_axis = data->z_axis>>4;

    if (res < 6) {
        DEBUG("[!!failed!!]\n");
        return -1;
    }
    DEBUG("[done]\n");

    return 0;
}

int lsm303c_read_mag(const lsm303c_t *dev, lsm303c_3d_data_t *data)
{
    int res;
    uint8_t tmp;

    DEBUG("read ... ");

    i2c_acquire(dev->i2c);

    /* trigger a conversion */
    i2c_write_reg(dev->i2c, dev->mag_address,
                       LSM303C_REG_CTRL3_M, LSM303C_CTRL3_M_SINGLE_CONV);

    res = i2c_read_reg(dev->i2c, dev->mag_address,
                       LSM303C_REG_OUT_X_L_M, &tmp);
    data->x_axis = tmp;
    res += i2c_read_reg(dev->i2c, dev->mag_address,
                        LSM303C_REG_OUT_X_H_M, &tmp);
    data->x_axis |= tmp<<8;
    res += i2c_read_reg(dev->i2c, dev->mag_address,
                       LSM303C_REG_OUT_Y_L_M, &tmp);
    data->y_axis = tmp;
    res += i2c_read_reg(dev->i2c, dev->mag_address,
                        LSM303C_REG_OUT_Y_H_M, &tmp);
    data->y_axis |= tmp<<8;
    res += i2c_read_reg(dev->i2c, dev->mag_address,
                       LSM303C_REG_OUT_Z_L_M, &tmp);
    data->z_axis = tmp;
    res += i2c_read_reg(dev->i2c, dev->mag_address,
                        LSM303C_REG_OUT_Z_H_M, &tmp);
    data->z_axis |= tmp<<8;
    i2c_release(dev->i2c);

    if (res < 6) {
        DEBUG("[!!failed!!]\n");
        return -1;
    }
    DEBUG("[done]\n");

    return 0;
}

int lsm303c_disable(const lsm303c_t *dev)
{
    int res;

    i2c_acquire(dev->i2c);
    res = i2c_write_reg(dev->i2c, dev->acc_address,
                        LSM303C_REG_CTRL1_A, LSM303C_CTRL1_A_POWEROFF);
    res += i2c_write_reg(dev->i2c, dev->mag_address,
                        LSM303C_REG_CTRL3_M, LSM303C_CTRL3_M_POWERDOWN);
    i2c_release(dev->i2c);

    return (res < 2) ? -1 : 0;
}

int lsm303c_enable(const lsm303c_t *dev)
{
    return -1;
    // int res;
    // uint8_t tmp = (LSM303C_CTRL1_A_XEN
    //               | LSM303C_CTRL1_A_YEN
    //               | LSM303C_CTRL1_A_ZEN
    //               | LSM303C_CTRL1_A_10HZ;
    // i2c_acquire(dev->i2c);
    // res = i2c_write_reg(dev->i2c, dev->acc_address, LSM303C_REG_CTRL1_A, tmp);
    //
    // tmp = (LSM303C_CTRL4_A_BDU| LSM303C_CTRL4_A_SCALE_2G | LSM303C_CTRL4_A_HR);
    // res += i2c_write_reg(dev->i2c, dev->acc_address, LSM303C_REG_CTRL4_A, tmp);
    // res += i2c_write_reg(dev->i2c, dev->acc_address, LSM303C_REG_CTRL3_A, LSM303C_CTRL3_A_I1_DRDY1);
    // gpio_init(dev->acc_pin, GPIO_IN);
    //
    // tmp = LSM303C_TEMP_EN | LSM303C_TEMP_SAMPLE_75HZ;
    // res += i2c_write_reg(dev->i2c, dev->mag_address, LSM303C_REG_CRA_M, tmp);
    //
    // res += i2c_write_reg(dev->i2c, dev->mag_address,
    //                     LSM303C_REG_CRB_M, LSM303C_GAIN_5);
    //
    // res += i2c_write_reg(dev->i2c, dev->mag_address,
    //                     LSM303C_REG_MR_M, LSM303C_MAG_MODE_CONTINUOUS);
    // i2c_release(dev->i2c);
    //
    // gpio_init(dev->mag_pin, GPIO_IN);
    //
    // return (res < 6) ? -1 : 0;
}
