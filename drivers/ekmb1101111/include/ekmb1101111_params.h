/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_ekmb1101111
 *
 * @{
 * @file
 * @brief       Default configuration for EKMB1101111 devices
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 */

#ifndef EKMB1101111_PARAMS_H
#define EKMB1101111_PARAMS_H

#include "board.h"
#include "ekmb1101111.h"
#include "saul_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Set default configuration parameters for the APDS9007 driver
 * @{
 */
#ifndef EKMB1101111_PARAMS_BOARD
#define EKMB1101111_PARAMS_DEFAULT  { .gpio = GPIO_PIN(PA, 6) }
#endif 
/**@}*/

/**
 * @brief   EKMB1101111 configuration
 */
static const ekmb1101111_params_t ekmb1101111_params[] =
{
#ifdef EKMB1101111_PARAMS_BOARD
    EKMB1101111_PARAMS_BOARD,
#else
    EKMB1101111_PARAMS_DEFAULT,
#endif
};

/**
 * @brief   Additional meta information to keep in the SAUL registry
 */
static const saul_reg_info_t ekmb1101111_saul_info[] =
{
    {
        .name = "ekmb110",
    },
};

#ifdef __cplusplus
}
#endif

#endif /* EKMB1101111_PARAMS_H */
/** @} */
