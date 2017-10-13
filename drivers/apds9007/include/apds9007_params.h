/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_apds9007
 *
 * @{
 * @file
 * @brief       Default configuration for APDS9007 devices
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 */

#ifndef APDS9007_PARAMS_H
#define APDS9007_PARAMS_H

#include "board.h"
#include "apds9007.h"
#include "saul_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Set default configuration parameters for the APDS9007 driver
 * @{
 */
#ifndef APDS9007_PARAMS_BOARD
#define APDS9007_PARAMS_DEFAULT    { .gpio  = GPIO_PIN(PA,28), \
                                     .adc   = ADC_PIN_PA08, \
                                     .res   = ADC_RES_16BIT }
#endif 
/**@}*/

/**
 * @brief   APDS9007 configuration
 */
static const apds9007_params_t apds9007_params[] =
{
#ifdef APDS9007_PARAMS_BOARD
    APDS9007_PARAMS_BOARD,
#else
    APDS9007_PARAMS_DEFAULT,
#endif
};

/**
 * @brief   Additional meta information to keep in the SAUL registry
 */
static const saul_reg_info_t apds9007_saul_info[] =
{
    {
        .name = "apds9007",
    },
};

#ifdef __cplusplus
}
#endif

#endif /* APDS9007_PARAMS_H */
/** @} */
