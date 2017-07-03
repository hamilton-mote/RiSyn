/*
 * Copyright (C) 2017 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/*
 * @ingroup     auto_init_saul
 * @{
 *
 * @file
 * @brief       Auto initialization for APDS9007 devices
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 *
 * @}
 */

#ifdef MODULE_APDS9007

#include "log.h"
#include "saul_reg.h"
#include "apds9007_params.h"

/**
 * @brief   Define the number of configured sensors
 */
#define APDS9007_NUM    (sizeof(apds9007_params)/sizeof(apds9007_params[0]))

/**
 * @brief   Allocate memory for the device descriptors
 */
static apds9007_t apds9007_devs[APDS9007_NUM];

/**
 * @brief   Memory for the SAUL registry entries
 */
static saul_reg_t saul_entries[APDS9007_NUM];

/**
 * @brief   Reference the driver struct
 * @{
 */
extern saul_driver_t apds9007_saul_light_driver;
/** @} */


void auto_init_apds9007(void)
{
    for (unsigned i = 0; i < APDS9007_NUM; i++) {
        LOG_DEBUG("[auto_init_saul] initializing apds9007 #%u\n", i);

        int res = apds9007_init(&apds9007_devs[i], &apds9007_params[i]);
        if (res != 0) {
            LOG_ERROR("[auto_init_saul] error initializing apds9007 #%u\n", i);
        }
        else {
            saul_entries[i].dev = &(apds9007_devs[i]);
            saul_entries[i].name = apds9007_saul_info[i].name;
            saul_entries[i].driver = &apds9007_saul_light_driver;
            saul_reg_add(&(saul_entries[i]));
        }
    }
}

#else
typedef int dont_be_pedantic;
#endif /* MODULE_APDS9007 */
