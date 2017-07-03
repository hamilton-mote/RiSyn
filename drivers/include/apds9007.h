/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_apds9007 light sensor
 * @ingroup     drivers_sensors
 *
 * The connection between the MCU and the APDS9007 is based on the
 * GPIO-interface.
 *
 * @{
 *
 * @file
 * @brief       Driver for the APDS9007 light sensor
 *
 * @author      Hyung-Sin <hs.kim@berkeley.edu>
 */

#ifndef APDS9007_H_
#define APDS9007_H_

#include <stdint.h>
#include "periph/gpio.h"
#include "periph/adc.h"

#ifdef __cplusplus
extern "C" {
#endif



#ifndef APDS9007_STABILIZATION_TIME
#define APDS9007_STABILIZATION_TIME 20000UL
#endif

/**
 * @brief   Parameters needed for device initialization
 */
typedef struct {
    gpio_t    gpio;            /**< GPIO pin that sensor is connected to */
    adc_t     adc;             /**< ADC channel that sensor is connected to */
    adc_res_t res;             /**< ADC resolution that sensor uses */
} apds9007_params_t;

/**
  * @brief   Device descriptor for a APDS9007 device
  * @{
  */
typedef struct {
    apds9007_params_t p;
} apds9007_t;
/** @} */

/**
 * @brief   Initialize an APDS9007 device
 *
 * @param[out] dev          device descriptor
 * @param[in] params        I2C bus the device is connected to
 *
 * The valid address range is 0x1E - 0x1F depending on the configuration of the
 * address pins SA0 and SA1.
 *
 * @return                   0 on success
 * @return                  -1 on error
 * @return                  -2 on invalid address
 */
int apds9007_init(apds9007_t* dev, const apds9007_params_t* params);

int apds9007_set_active(apds9007_t* dev);

int apds9007_set_idle(apds9007_t* dev);

int apds9007_read(apds9007_t* dev, int16_t *light);
#ifdef __cplusplus
}
#endif

/** @} */
#endif /* APDS9007_H_ */
