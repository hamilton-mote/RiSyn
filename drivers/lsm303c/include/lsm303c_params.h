/*
 * Copyright (C) 2018 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_lsm303c
 *
 * @{
 * @file
 * @brief       Default configuration for LSM303C devices
 *
 * @author     Michael Andersen <m.andersen@cs.berkeley.edu>
 */

#ifndef LSM303C_PARAMS_H
#define LSM303C_PARAMS_H

#include "board.h"
#include "lsm303c.h"
#include "saul_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Set default configuration parameters
 * @{
 */
#ifndef LSM303C_PARAM_I2C
#define LSM303C_PARAM_I2C            I2C_DEV(0)
#endif
#ifndef LSM303C_PARAM_ACC_ADDR
#define LSM303C_PARAM_ACC_ADDR       (0x1d)
#endif
#ifndef LSM303C_PARAM_ACC_RATE
#define LSM303C_PARAM_ACC_RATE       (LSM303C_ACC_SAMPLE_RATE_ONESHOT)
#endif
#ifndef LSM303C_PARAM_ACC_SCALE
#define LSM303C_PARAM_ACC_SCALE      (LSM303C_ACC_SCALE_2G)
#endif
#ifndef LSM303C_PARAM_MAG_ADDR
#define LSM303C_PARAM_MAG_ADDR       (0x1e)
#endif
#ifndef LSM303C_PARAM_MAG_RATE
#define LSM303C_PARAM_MAG_RATE       (LSM303C_MAG_SAMPLE_RATE_5HZ)
#endif


#define LSM303C_PARAMS_DEFAULT       { .i2c       = LSM303C_PARAM_I2C, \
                                          .acc_addr  = LSM303C_PARAM_ACC_ADDR, \
                                          .acc_rate  = LSM303C_PARAM_ACC_RATE, \
                                          .acc_scale = LSM303C_PARAM_ACC_SCALE, \
                                          .mag_addr  = LSM303C_PARAM_MAG_ADDR, \
                                          .mag_rate  = LSM303C_PARAM_MAG_RATE }
/**@}*/

/**
 * @brief   Allocate some memory to store the actual configuration
 */
static const lsm303c_params_t lsm303c_params[] =
{
#ifdef LSM303C_PARAMS_CUSTOM
    LSM303C_PARAMS_CUSTOM,
#else
    LSM303C_PARAMS_DEFAULT,
#endif
};

/**
 * @brief   Additional meta information to keep in the SAUL registry
 */
static const saul_reg_info_t lsm303c_saul_info[] =
{
    { .name = "lsm303c" }
};

#ifdef __cplusplus
}
#endif

#endif /* LSM303C_PARAMS_H */
/** @} */
