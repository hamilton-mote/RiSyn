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
 * @brief       ISL29035 adaption to the RIOT actuator/sensor interface
 *
 * @author      Michael Andersen <m.andersen@berkeley.edu>
 *
 * @}
 */

#include <string.h>

#include "saul.h"
#include "isl29035.h"

static int read(const void *dev, phydat_t *res)
{
    res->val[0] = (int16_t)isl29035_read((const isl29035_t *)dev);
    memset(&(res->val[1]), 0, 2 * sizeof(int16_t));
    res->unit = UNIT_CD;
    res->scale = 0;
    return 1;
}

const saul_driver_t isl29035_saul_driver = {
    .read = read,
    .write = saul_notsup,
    .type = SAUL_SENSE_LIGHT,
};
