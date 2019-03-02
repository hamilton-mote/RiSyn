/*
 * Copyright (C) 2018 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_lsm303c
 * @{
 *
 * @file
 * @brief       Device driver implementation for the LSM303C 3D accelerometer/magnetometer.
 *
 * @author      Michael Andersen <m.andersen@cs.berkeley.edu>
 *
 * @}
 */

#ifndef LSM303C_H
#define LSM303C_H

#include <stdint.h>
#include "periph/i2c.h"
#include "periph/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name    The sensors default I2C address
 * @{
 */
#define LSM303C_ACC_DEFAULT_ADDRESS        (0x19)
#define LSM303C_MAG_DEFAULT_ADDRESS        (0x1e)
/** @} */

/**
 * @brief   Possible accelerometer sample rates
 */
typedef enum {
    LSM303C_ACC_SAMPLE_RATE_10HZ             = 0x10, /**< 10Hz sample rate      */
    LSM303C_ACC_SAMPLE_RATE_50HZ             = 0x20, /**< 50Hz sample rate      */
    LSM303C_ACC_SAMPLE_RATE_100HZ            = 0x30, /**< 100Hz sample rate     */
    LSM303C_ACC_SAMPLE_RATE_200HZ            = 0x40, /**< 200Hz sample rate     */
    LSM303C_ACC_SAMPLE_RATE_400HZ            = 0x50, /**< 400Hz sample rate     */
    LSM303C_ACC_SAMPLE_RATE_800HZ            = 0x60, /**< 800Hz sample rate     */
    LSM303C_ACC_SAMPLE_RATE_ONESHOT          = 0xFF  /**< Sample once per read call */
} lsm303c_acc_sample_rate_t;

/**
 * @brief   Possible accelerometer scales
 */
typedef enum {
    LSM303C_ACC_SCALE_2G     = 0x00, /**< +- 2g range */
    LSM303C_ACC_SCALE_4G     = 0x20, /**< +- 8g range */
    LSM303C_ACC_SCALE_8G    = 0x30, /**< +-16g range */
} lsm303c_acc_scale_t;

/**
 * @brief   Possible magnetometer sample rates
 */
typedef enum {
    LSM303C_MAG_SAMPLE_RATE_0_625HZ  = 0x00, /**< 0.625Hz sample rate */
    LSM303C_MAG_SAMPLE_RATE_1_25HZ   = 0x04, /**< 1.25 Hz sample rate */
    LSM303C_MAG_SAMPLE_RATE_2_5HZ    = 0x08, /**< 2.5 Hz sample rate */
    LSM303C_MAG_SAMPLE_RATE_5HZ      = 0x0c, /**< 5 Hz sample rate */
    LSM303C_MAG_SAMPLE_RATE_10HZ     = 0x10, /**< 10  Hz sample rate */
    LSM303C_MAG_SAMPLE_RATE_20HZ     = 0x14, /**< 20  Hz sample rate */
    LSM303C_MAG_SAMPLE_RATE_40HZ     = 0x18, /**< 40  Hz sample rate */
    LSM303C_MAG_SAMPLE_RATE_80HZ     = 0x1c, /**< 80 Hz sample rate */
    LSM303C_MAG_SAMPLE_RATE_ONESHOT  = 0xFF  /**< Sample once per read call */
} lsm303c_mag_sample_rate_t;

/**
 * @brief   3d data container
 */
typedef struct {
    int16_t x_axis;     /**< holds the x axis value. */
    int16_t y_axis;     /**< holds the y axis value. */
    int16_t z_axis;     /**< holds the z axis value. */
} lsm303c_3d_data_t;

/**
 * @brief   Device descriptor for LSM303C sensors
 */
typedef struct {
    i2c_t i2c;                          /**< I2C device                  */
    uint8_t acc_address;                /**< accelerometer's I2C address */
    uint8_t mag_address;                /**< magnetometer's I2C address  */
    lsm303c_acc_scale_t acc_scale;      /**< accelerometer scale factor */
    lsm303c_acc_sample_rate_t acc_sr;   /**< accelerometer sample rate */
    lsm303c_mag_sample_rate_t mag_sr;   /**< magnetometer sample rate */
} lsm303c_t;

/**
 * @brief   Data structure holding all the information needed for initialization
 */
typedef struct {
    i2c_t i2c;                              /**< I2C bus used */
    uint8_t acc_addr;                       /**< accelerometer I2C address */
    lsm303c_acc_sample_rate_t acc_rate;  /**< accelerometer sample rate */
    lsm303c_acc_scale_t acc_scale;       /**< accelerometer scale factor */
    uint8_t mag_addr;                       /**< magnetometer I2C address */
    lsm303c_mag_sample_rate_t mag_rate;  /**< magnetometer sample rate */
} lsm303c_params_t;

/**
 * @brief   Initialize a new LSM303C device
 *
 * @param[in] dev               device descriptor of an LSM303C device
 * @param[in] i2c               I2C device the sensor is connected to
 * @param[in] acc_address       I2C address of the accelerometer
 * @param[in] acc_sample_rate   accelerometer sample rate
 * @param[in] acc_scale         accelerometer scale (from +- 2g to +-16g)
 * @param[in] mag_address       I2C address of the magnetometer
 * @param[in] mag_sample_rate   magnetometer sample rate
 *
 * @return                  0 on success
 * @return                  -1 on error
 */
int lsm303c_init(lsm303c_t *dev, i2c_t i2c,
                    uint8_t acc_address,
                    lsm303c_acc_sample_rate_t acc_sample_rate,
                    lsm303c_acc_scale_t acc_scale,
                    uint8_t mag_address,
                    lsm303c_mag_sample_rate_t mag_sample_rate);

/**
 * @brief   Read a accelerometer value from the sensor.
 *
 * @details This function provides raw acceleration data. To get the
 *          corresponding values in g please refer to the following
 *          table:
 *                measurement range | factor
 *              --------------------+---------
 *                          +- 2g   |  61*10^-6
 *                          +- 4g   |  122*10^-6
 *                          +- 8g   |  244*10^-6
 *
 * @param[in]  dev      device descriptor of an LSM303C device
 * @param[out] data     the measured accelerometer data
 *
 * @return              0 on success
 * @return              -1 on error
 */
int lsm303c_read_acc(const lsm303c_t *dev, lsm303c_3d_data_t *data);

/**
 * @brief   Read a magnetometer value from the sensor.
 *
 * @details This function returns raw magnetic data. To get the
 *          corresponding values in gauss please refer to the following
 *          table:
 *                measurement range |  factor
 *              --------------------+---------
 *                     any          |  580*10^-6
 *
 * @param[in]  dev      device descriptor of an LSM303C device
 * @param[out] data     the measured magnetometer data
 *
 * @return              0 on success
 * @return              -1 on error
 */
int lsm303c_read_mag(const lsm303c_t *dev, lsm303c_3d_data_t *data);


/**
 * @brief   Enable the given sensor
 *
 * @param[in] dev       device descriptor of an LSM303C device
 *
 * @return              0 on success
 * @return              -1 on error
 */
int lsm303c_enable(const lsm303c_t *dev);

/**
 * @brief   Disable the given sensor
 *
 * @param[in] dev       device descriptor of an LSM303C device
 *
 * @return              0 on success
 * @return              -1 on error
 */
int lsm303c_disable(const lsm303c_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* LSM303C_H */
 /** @} */
