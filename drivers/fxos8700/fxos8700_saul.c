/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     driver_fxos8700
 * @{
 *
 * @file
 * @brief       FXOS8700 adaption to the RIOT actuator/sensor interface
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 *
 * @}
 */

#include <string.h>

#include "saul.h"
#include "fxos8700.h"

static int read_mag(void *dev, phydat_t *res)
{
    fxos8700_t *d = (fxos8700_t *)dev;
    if (fxos8700_read_mag(d, (fxos8700_measurement_mag_t *)res)) {
			/* Read failure */			
			return -ECANCELED;
		}

    res->unit = UNIT_GS;
    res->scale = -3;
    return 3;
}

static int read_acc(void *dev, phydat_t *res)
{
    fxos8700_t *d = (fxos8700_t *)dev;
    if (fxos8700_read_acc(d, (fxos8700_measurement_acc_t *)res)) {
			/* Read failure */			
			return 0;
		}

    res->unit = UNIT_G;
    res->scale = -3;
    return 3;
}

const saul_driver_t fxos8700_saul_mag_driver = {
    .read = read_mag,
    .write = saul_notsup,
    .type = SAUL_SENSE_MAG,
};

const saul_driver_t fxos8700_saul_acc_driver = {
    .read = read_acc,
    .write = saul_notsup,
    .type = SAUL_SENSE_ACCEL,
};
