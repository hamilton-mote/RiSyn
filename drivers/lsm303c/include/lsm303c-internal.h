/*
 * Copyright (C) 2018 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     driver_lsm303c
 *
 * @{
 *
 * @file
 * @brief       Definitions for the LSM303C 3D accelerometer/magnetometer
 *
 * @author      Michael Andersen <m.andersen@cs.berkeley.edu>
 */

#ifndef LSM303C_INTERNAL_H
#define LSM303C_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name LSM303C accelerometer registers
 * @{
 */
#define LSM303C_REG_WHO_AM_I_A           (0x0f)
#define LSM303C_REG_ACT_THS_A            (0x1e)
#define LSM303C_REG_ACT_DUR_A            (0x1f)
#define LSM303C_REG_CTRL1_A              (0x20)
#define LSM303C_REG_CTRL2_A              (0x21)
#define LSM303C_REG_CTRL3_A              (0x22)
#define LSM303C_REG_CTRL4_A              (0x23)
#define LSM303C_REG_CTRL5_A              (0x24)
#define LSM303C_REG_CTRL6_A              (0x25)
#define LSM303C_REG_CTRL7_A              (0x26)
#define LSM303C_REG_STATUS_A             (0x27)
#define LSM303C_REG_OUT_X_L_A            (0x28)
#define LSM303C_REG_OUT_X_H_A            (0x29)
#define LSM303C_REG_OUT_Y_L_A            (0x2a)
#define LSM303C_REG_OUT_Y_H_A            (0x2b)
#define LSM303C_REG_OUT_Z_L_A            (0x2c)
#define LSM303C_REG_OUT_Z_H_A            (0x2d)
#define LSM303C_REG_FIFO_CTRL            (0x2e)
#define LSM303C_REG_FIFO_SRC             (0x2f)
#define LSM303C_REG_IG_CFG1_A            (0x30)
#define LSM303C_REG_IG_SRC1_A            (0x31)
#define LSM303C_REG_IG_THS_X1_A          (0x32)
#define LSM303C_REG_IG_THS_Y1_A          (0x33)
#define LSM303C_REG_IG_THS_Z1_A          (0x34)
#define LSM303C_REG_IG_DUR1_A            (0x35)
#define LSM303C_REG_IG_CFG2_A            (0x36)
#define LSM303C_REG_IG_SRC2_A            (0x37)
#define LSM303C_REG_IG_THS2_A            (0x38)
#define LSM303C_REG_IG_DUR2_A            (0x39)
#define LSM303C_REG_XL_REFERENCE         (0x3A)
#define LSM303C_REG_XH_REFERENCE         (0x3B)
#define LSM303C_REG_YL_REFERENCE         (0x3C)
#define LSM303C_REG_YH_REFERENCE         (0x3D)
#define LSM303C_REG_ZL_REFERENCE         (0x3E)
#define LSM303C_REG_ZH_REFERENCE         (0x3F)
/** @} */

/**
 * @name Masks for the LSM303C CTRL1_A register
 * @{
 */
#define LSM303C_CTRL1_A_XEN              (0x01)
#define LSM303C_CTRL1_A_YEN              (0x02)
#define LSM303C_CTRL1_A_ZEN              (0x04)
#define LSM303C_CTRL1_A_BLOCK_DATA_UPD   (0x08)
#define LSM303C_CTRL1_A_POWEROFF         (0x00)
#define LSM303C_CTRL1_A_10HZ             (0x10)
#define LSM303C_CTRL1_A_50HZ             (0x20)
#define LSM303C_CTRL1_A_100HZ            (0x30)
#define LSM303C_CTRL1_A_200HZ            (0x40)
#define LSM303C_CTRL1_A_400HZ            (0x50)
#define LSM303C_CTRL1_A_800HZ            (0x60)
#define LSM303C_CTRL1_A_HIGHRES          (0x80)
/** @} */


/**
 * @name LSM303C magnetometer registers
 * @{
 */
#define LSM303C_REG_WHO_AM_I_M       (0x0f)
#define LSM303C_REG_CTRL1_M          (0x20)
#define LSM303C_REG_CTRL2_M          (0x21)
#define LSM303C_REG_CTRL3_M          (0x22)
#define LSM303C_REG_CTRL4_M          (0x23)
#define LSM303C_REG_CTRL5_M          (0x24)
#define LSM303C_REG_STATUS_M         (0x27)
#define LSM303C_REG_OUT_X_L_M        (0x28)
#define LSM303C_REG_OUT_X_H_M        (0x29)
#define LSM303C_REG_OUT_Y_L_M        (0x2a)
#define LSM303C_REG_OUT_Y_H_M        (0x2b)
#define LSM303C_REG_OUT_Z_L_M        (0x2c)
#define LSM303C_REG_OUT_Z_H_M        (0x2d)
#define LSM303C_REG_TEMP_L_M         (0x2e)
#define LSM303C_REG_TEMP_H_M         (0x2f)
#define LSM303C_REG_INT_CFG_M        (0x30)
#define LSM303C_REG_INT_SRC_M        (0x31)
#define LSM303C_REG_INT_THS_L_M      (0x32)
#define LSM303C_REG_INT_THS_H_M      (0x33)
/** @} */

/**
 * @name Masks for the LSM303C CTRL3_M register
 * @{
 */
#define LSM303C_CTRL3_M_CONTINUOUS       (0x00)
#define LSM303C_CTRL3_M_SINGLE_CONV      (0x01)
#define LSM303C_CTRL3_M_POWERDOWN        (0x02)
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* LSM303C_INTERNAL_H */
/** @} */
