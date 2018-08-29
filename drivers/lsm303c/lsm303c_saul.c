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

#include <string.h>

#include "saul.h"
#include "lsm303c.h"

static int read_acc(const void *dev, phydat_t *res)
{
    const lsm303c_t *d = (const lsm303c_t *)dev;
    lsm303c_read_acc(d, (lsm303c_3d_data_t *)res);

    /* normalize result */
    int fac = (1 << (d->acc_scale >> 4));
    for (int i = 0; i < 3; i++) {
        res->val[i] *= fac;
    }

    res->unit = UNIT_G;
    res->scale = -3;
    return 3;
}

static int read_mag(const void *dev, phydat_t *res)
{
    const lsm303c_t *d = (const lsm303c_t *)dev;

    lsm303c_read_mag(d, (lsm303c_3d_data_t *)res);

    for (int i = 0; i < 3; i++) {
        int32_t tmp = res->val[i] * 580;
        tmp /= 1000;
        res->val[i] = (int16_t)tmp;
    }

    res->unit = UNIT_GS;
    res->scale = -3;
    return 3;
}

const saul_driver_t lsm303c_saul_acc_driver = {
    .read = read_acc,
    .write = saul_notsup,
    .type = SAUL_SENSE_ACCEL,
};

const saul_driver_t lsm303c_saul_mag_driver = {
    .read = read_mag,
    .write = saul_notsup,
    .type = SAUL_SENSE_MAG,
};
