/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_tmp006
 *
 * @{
 * @file
 * @brief       Default configuration for TMP006 devices
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 */

#ifndef TMP006_PARAMS_H
#define TMP006_PARAMS_H

#include "board.h"
#include "tmp006.h"
#include "saul_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   default configuration parameters for the TMP006 driver
 * @{
 */
#define TMP006_PARAMS_DEFAULT   { .i2c  = I2C_0, \
                                  .addr = 0x44, \
                                  .conv_rate  = TMP006_CONFIG_CR_AS2 }
/**@}*/

/**
 * @brief   TMP006 configuration
 */
static const tmp006_params_t tmp006_params[] =
{
#ifdef TMP006_PARAMS_BOARD
    TMP006_PARAMS_BOARD,
#else
    TMP006_PARAMS_DEFAULT,
#endif
};

/**
 * @brief   Additional meta information to keep in the SAUL registry
 */
static const saul_reg_info_t tmp006_saul_info[] =
{
    {
        .name = "tmp006",
    },
};

#ifdef __cplusplus
}
#endif

#endif /* TMP006_PARAMS_H */
/** @} */
