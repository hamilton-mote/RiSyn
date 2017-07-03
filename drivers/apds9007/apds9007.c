/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/**
 * @ingroup     drivers_apds9007
 * @{
 *
 * @file
 * @brief       Driver for the APDS9007 Light Sensor.
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 *
 * @}
 */

#include <string.h>

#include "apds9007.h"
#include "xtimer.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"


int apds9007_set_active(apds9007_t *dev)
{
	gpio_write(dev->p.gpio, 0);
	return 0;
} 

int apds9007_set_idle(apds9007_t *dev)
{
	gpio_write(dev->p.gpio, 1);
  return 0;
}

int apds9007_init(apds9007_t *dev, const apds9007_params_t *params)
{    
		dev->p.gpio = params->gpio;
		dev->p.adc  = params->adc;
		dev->p.res  = params->res;
    gpio_init(params->gpio, GPIO_OUT);
		adc_init(params->adc);
		apds9007_set_idle(dev);
    return 0;
}

int apds9007_read(apds9007_t *dev, int16_t *light)
{
	apds9007_set_active(dev);
	xtimer_usleep(APDS9007_STABILIZATION_TIME);
	*light = (int16_t) adc_sample(dev->p.adc, dev->p.res);
	apds9007_set_idle(dev);
  return 0;
}

