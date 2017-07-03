/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     driver_ekmb1101111
 * @{
 *
 * @file
 * @brief       EKMB1101111 adaption to the RIOT actuator/sensor interface
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 *
 * @}
 */

#include <string.h>

#include "saul.h"
#include "ekmb1101111.h"

static int read_occup(void *dev, phydat_t *res)
{
    ekmb1101111_t *d = (ekmb1101111_t *)dev;
    if (ekmb1101111_read(d, &(res->val[0]))) {
			/* Read failure */
			return -ECANCELED;
		}
    memset(&(res->val[1]), 0, 2 * sizeof(int16_t));
    res->unit = UNIT_PERCENT;
    res->scale = -2;
    return 1;
}

const saul_driver_t ekmb1101111_saul_occup_driver = {
    .read = read_occup,
    .write = saul_notsup,
    .type = SAUL_SENSE_OCCUP,
};
