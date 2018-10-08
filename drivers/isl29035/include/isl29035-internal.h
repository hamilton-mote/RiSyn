/*
 * Copyright (C) 2018 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     driver_isl29035
 * @{
 *
 * @file
 * @brief       Definitions for the ISL29035 light sensor
 *
 * @author      Michael Andersen <m.andersen@berkeley.edu>
 */

#ifndef ISL29035_INTERNAL_H
#define ISL29035_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name ISL29035 registers
 * @{
 */
#define ISL29035_REG_CMD1       0x00
#define ISL29035_REG_CMD2       0x01
#define ISL29035_REG_LDATA      0x02
#define ISL29035_REG_HDATA      0x03
/** @} */


/**
 * @name Resolution options
 * @{
 */
#define ISL29035_RES_INT_16     0x00
#define ISL29035_RES_INT_12     0x04
#define ISL29035_RES_INT_8      0x08
#define ISL29035_RES_INT_4      0x0c
/** @} */

/**
 * @name Range options
 * @{
 */
#define ISL29035_RANGE_1        0x00
#define ISL29035_RANGE_2        0x01
#define ISL29035_RANGE_3        0x02
#define ISL29035_RANGE_4        0x03
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ISL29035_INTERNAL_H */
/** @} */
