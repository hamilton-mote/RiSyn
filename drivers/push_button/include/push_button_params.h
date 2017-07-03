/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_push_button
 *
 * @{
 * @file
 * @brief       Default configuration for PUSH_BUTTON devices
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 */

#ifndef PUSH_BUTTON_PARAMS_H
#define PUSH_BUTTON_PARAMS_H

#include "board.h"
#include "push_button.h"
#include "saul_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Set default configuration parameters for the PUSH_BUTTON driver
 * @{
 */
#ifndef PUSH_BUTTON_PARAMS_BOARD
#define PUSH_BUTTON_PARAMS_DEFAULT  { .gpio = GPIO_PIN(PA, 19) }
#endif 
/**@}*/

/**
 * @brief   PUSH_BUTTON configuration
 */
static const push_button_params_t push_button_params[] =
{
#ifdef PUSH_BUTTON_PARAMS_BOARD
    PUSH_BUTTON_PARAMS_BOARD,
#else
    PUSH_BUTTON_PARAMS_DEFAULT,
#endif
};

/**
 * @brief   Additional meta information to keep in the SAUL registry
 */
static const saul_reg_info_t push_button_saul_info[] =
{
    {
        .name = "button",
    },
};

#ifdef __cplusplus
}
#endif

#endif /* PUSH_BUTTON_PARAMS_H */
/** @} */
