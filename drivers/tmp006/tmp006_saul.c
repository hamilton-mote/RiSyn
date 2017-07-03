/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     driver_tmp006
 * @{
 *
 * @file
 * @brief       TMP006 adaption to the RIOT actuator/sensor interface
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 *
 * @}
 */

#include <string.h>

#include "saul.h"
#include "tmp006.h"

static int read_ambtemp(void *dev, phydat_t *res)
{
    tmp006_t *d = (tmp006_t *)dev;
		if (tmp006_read(d, &(res->val[0]))) {
			/* Read failure */
			return -ECANCELED;
		}
    memset(&(res->val[1]), 0, 2 * sizeof(int16_t));
    res->unit = UNIT_TEMP_C;
    res->scale = -2;
    return 1;
}

const saul_driver_t tmp006_saul_ambtemp_driver = {
    .read = read_ambtemp,
    .write = saul_notsup,
    .type = SAUL_SENSE_AMBTEMP,
};
