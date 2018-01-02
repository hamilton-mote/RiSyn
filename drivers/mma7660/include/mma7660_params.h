/*
 * Copyright (C) 2016 University of California, Berkeley
 * Copyright (C) 2016 Michael Andersen <m.andersen@cs.berkeley.edu>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/**
 * @ingroup     drivers_mma7660
 * @{
 *
 * @file
 * @brief       Default configuration for MMA7660 accelerometer.
 *
 * @author      Michael Andersen <m.andersen@cs.berkeley.edu>
 *
 */

#ifndef MMA7660_PARAMS_H
#define MMA7660_PARAMS_H

#include "board.h"
#include "mma7660.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Set default configuration parameters for MMA7660 devices
 * @{
 */
#ifndef MMA7660_PARAM_I2C
#define MMA7660_PARAM_I2C         I2C_DEV(0)
#endif
#ifndef MMA7660_PARAM_ADDR
#define MMA7660_PARAM_ADDR        (MMA7660_ADDR)
#endif

#define MMA7660_PARAMS_DEFAULT    {.i2c = MMA7660_PARAM_I2C, \
                                   .addr = MMA7660_PARAM_ADDR}
/**@}*/

/**
 * @brief   MMA7660 configuration
 */
static const mma7660_params_t mma7660_params[] =
{
#ifdef MMA7660_PARAMS_BOARD
    MMA7660_PARAMS_BOARD,
#else
    MMA7660_PARAMS_DEFAULT,
#endif
};

#ifdef __cplusplus
}
#endif

#endif /* MMA7660_PARAMS_H */
/** @} */
