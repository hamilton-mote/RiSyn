/*
 * Copyright (C) 2018 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */


/**
 * @defgroup    drivers_isl29035 ISL29035 light sensor
 * @ingroup     drivers_sensors
 * @brief       Device driver for the ISL29035 light sensor
 * @{
 *
 * @file
 * @brief       Device driver interface for the ISL29035 light sensor
 *
 * @author      Michael Andersen <m.andersen@berkeley.edu>
 */

#ifndef ISL29035_H
#define ISL29035_H

#include <stdint.h>
#include "periph/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

 /**
  * @brief   The sensors default I2C address
  */
#define ISL29035_DEFAULT_ADDRESS        0x44

/**
 * @brief   Possible modes for the ISL29035 sensor
 */
typedef enum {
    ISL29035_MODE_POWEROFF = 0,                /**< turn the device off */
    ISL29035_MODE_AUTO_POWERDOWN_AMBIENT = 1,  /**< set sensor to detect ambient light */
    ISL29035_MODE_AUTO_POWERDOWN_IR = 2,       /**< set sensor to detect infrared light */
    ISL29035_MODE_CONTINUOUS_AMBIENT = 5,      /**< set sensor to detect ambient light continuously */
    ISL29035_MODE_CONTINUOUS_IR = 6            /**< set sensor to detect infrared light continuously */
} isl29035_mode_t;

/**
 * @brief   Possible range values for the ISL29035 sensor
 */
typedef enum {
    ISL29035_RANGE_1K = 0,      /**< set range to 0-1000 lux */
    ISL29035_RANGE_4K = 1,      /**< set range to 0-4000 lux */
    ISL29035_RANGE_16K = 2,     /**< set range to 0-16000 lux */
    ISL29035_RANGE_64K = 3      /**< set range to 0-64000 lux */
} isl29035_range_t;

/**
 * @brief   Device descriptor for ISL29035 sensors
 */
typedef struct {
    i2c_t i2c;                  /**< I2C device the sensor is connected to */
    uint8_t address;            /**< I2C bus address of the sensor */
    isl29035_mode_t mode;       /**< What mode the device was configured with */
    isl29035_range_t range;     /**< What range the device was configured with */
} isl29035_t;

/**
 * @brief   Data structure holding the full set of configuration parameters
 */
typedef struct {
    i2c_t i2c;                  /**< I2C bus the device is connected to */
    uint8_t addr;               /**< address on that bus */
    isl29035_range_t range;     /**< range setting to use */
    isl29035_mode_t mode;       /**< measurement mode to use */
} isl29035_params_t;

/**
 * @brief   Initialize a new ISL29035 device
 *
 * @param[in] dev       device descriptor of an ISL29035 device
 * @param[in] i2c       I2C device the sensor is connected to
 * @param[in] address   I2C address of the sensor
 * @param[in] range     measurement range
 * @param[in] mode      configure if sensor reacts to ambient or infrared light
 *
 * @return              0 on success
 * @return              -1 on error
 */
int isl29035_init(isl29035_t *dev, i2c_t i2c, uint8_t address,
                  isl29035_range_t range, isl29035_mode_t mode);

/**
 * @brief   Read a lighting value from the sensor, the result is given in lux
 *
 * @param[in] dev       device descriptor of an ISL29035 device
 *
 * @return              the measured brightness in lux
 * @return              -1 on error
 */
int isl29035_read(const isl29035_t *dev);

/**
 * @brief   Enable the given sensor
 *
 * @param[in] dev       device descriptor of an ISL29035 device
 *
 * @return              0 on success
 * @return              -1 on error
 */
int isl29035_enable(const isl29035_t *dev);

/**
 * @brief   Disable the given sensor
 *
 * @param[in] dev       device descriptor of an ISL29035 device
 *
 * @return              0 on success
 * @return              -1 on error
 */
int isl29035_disable(const isl29035_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* ISL29035_H */
/** @} */
