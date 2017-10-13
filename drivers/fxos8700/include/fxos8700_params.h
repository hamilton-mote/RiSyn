/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_fxos8700
 *
 * @{
 * @file
 * @brief       Default configuration for FXOS8700 devices
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 */

#ifndef FXOS8700_PARAMS_H
#define FXOS8700_PARAMS_H

#include "board.h"
#include "fxos8700.h"
#include "saul_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Default configuration parameters for the FXOS8700 driver
 * @{
 */
#define FXOS8700_PARAMS_DEFAULT   { .i2c  = I2C_0, \
                                    .addr = 0x1e }

/**@}*/

/**
 * @brief   FXOS8700 configuration
 */
static const fxos8700_params_t fxos8700_params[] =
{
#ifdef FXOS8700_PARAMS_BOARD
    FXOS8700_PARAMS_BOARD,
#else
    FXOS8700_PARAMS_DEFAULT,
#endif
};

/**
 * @brief   Additional meta information to keep in the SAUL registry
 */
static const saul_reg_info_t fxos8700_saul_info[] =
{
    {
        .name = "fxos8700",
    },
};

#ifdef __cplusplus
}
#endif

#endif /* FXOS8700_PARAMS_H */
/** @} */
