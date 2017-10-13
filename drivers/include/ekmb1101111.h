/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_ekmb1101111 PIR motion sensor
 * @ingroup     drivers_sensors
 *
 * The connection between the MCU and the EKMB1101111 is based on the
 * GPIO-interface.
 *
 * @{
 *
 * @file
 * @brief       Driver for the EKMB1101111 PIR motions sensor
 *
 * @author      Hyung-Sin <hs.kim@berkeley.edu>
 */

#ifndef EKMB1101111_H_
#define EKMB1101111_H_

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
} ekmb1101111_params_t;

/**
  * @brief   Device descriptor for a EKMB1101111 device
  * @{
  */
typedef struct {
    ekmb1101111_params_t p;
} ekmb1101111_t;
/** @} */

/**
 * @brief   Initialize an EKMB1101111 device
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
int ekmb1101111_init(ekmb1101111_t* dev, const ekmb1101111_params_t* params);

int ekmb1101111_read(ekmb1101111_t* dev, int16_t *light);
#ifdef __cplusplus
}
#endif

/** @} */
#endif /* EKMB1101111_H_ */
