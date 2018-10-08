/*
 * Copyright (C) 2018 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_isl29035
 *
 * @{
 * @file
 * @brief       Default configuration for ISL29035 devices
 *
 * @author      Michael Andersen <m.andersen@berkeley.edu>
 */

#ifndef ISL29035_PARAMS_H
#define ISL29035_PARAMS_H

#include "board.h"
#include "isl29035.h"
#include "saul_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Set default configuration parameters
 * @{
 */
#ifndef ISL29035_PARAM_I2C
#define ISL29035_PARAM_I2C              I2C_DEV(0)
#endif
#ifndef ISL29035_PARAM_ADDR
#define ISL29035_PARAM_ADDR             (0x44)
#endif
#ifndef ISL29035_PARAM_RANGE
#define ISL29035_PARAM_RANGE            (ISL29035_RANGE_16K)
#endif
#ifndef ISL29035_PARAM_MODE
#define ISL29035_PARAM_MODE             (ISL29035_MODE_AUTO_POWERDOWN_AMBIENT)
#endif

#define ISL29035_PARAMS_DEFAULT         { .i2c   = ISL29035_PARAM_I2C, \
                                          .addr  = ISL29035_PARAM_ADDR, \
                                          .range = ISL29035_PARAM_RANGE, \
                                          .mode  = ISL29035_PARAM_MODE,}
/**@}*/

/**
 * @brief   Allocate some memory to store the actual configuration
 */
static const isl29035_params_t isl29035_params[] =
{
#ifdef ISL29035_PARAMS_CUSTOM
    ISL29035_PARAMS_CUSTOM,
#else
    ISL29035_PARAMS_DEFAULT,
#endif
};

/**
 * @brief   Additional meta information to keep in the SAUL registry
 */
static const saul_reg_info_t isl29035_saul_info[] =
{
    { .name = "isl29035" }
};

#ifdef __cplusplus
}
#endif

#endif /* ISL29035_PARAMS_H */
/** @} */
