/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_push_button sensor
 * @ingroup     drivers_sensors
 *
 * The connection between the MCU and the PUSH_BUTTON is based on the
 * GPIO-interface.
 *
 * @{
 *
 * @file
 * @brief       Driver for the PUSH_BUTTON sensor
 *
 * @author      Hyung-Sin <hs.kim@berkeley.edu>
 */

#ifndef PUSH_BUTTON_H_
#define PUSH_BUTTON_H_

#include <stdint.h>
#include "periph/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief   Parameters needed for device initialization
 */
typedef struct {
    gpio_t    gpio;            /**< GPIO pin that sensor is connected to */
} push_button_params_t;

/**
  * @brief   Device descriptor for a PUSH_BUTTON device
  * @{
  */
typedef struct {
    push_button_params_t p;
} push_button_t;
/** @} */

/**
 * @brief   Initialize an PUSH_BUTTON device
 *
 * @param[out] dev          device descriptor
 * @param[in] params        GPIO bus the device is connected to
 *
 * The valid address range is 0x1E - 0x1F depending on the configuration of the
 * address pins SA0 and SA1.
 *
 * @return                   0 on success
 * @return                  -1 on error
 * @return                  -2 on invalid address
 */
int push_button_init(push_button_t* dev, const push_button_params_t* params);

int push_button_read(int16_t *button_events);
#ifdef __cplusplus
}
#endif

/** @} */
#endif /* PUSH_BUTTON_ */
