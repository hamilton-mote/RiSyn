/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     driver_push_button
 * @{
 *
 * @file
 * @brief       PUSH_BUTTON adaption to the RIOT actuator/sensor interface
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 *
 * @}
 */

#include <string.h>

#include "saul.h"
#include "push_button.h"

static int read_button(const void *dev, phydat_t *res)
{
    if (push_button_read(&(res->val[0]))) {
        /* Read failure */
	    return -ECANCELED;
    }
    memset(&(res->val[1]), 0, 2 * sizeof(int16_t));
    res->unit  = 0;
    res->scale = 0;
    return 1;
}

const saul_driver_t push_button_saul_driver = {
    .read  = read_button,
    .write = saul_notsup,
    .type  = SAUL_SENSE_BTN,
};
