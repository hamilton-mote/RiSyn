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
 * @brief       Auto initialization for TMP006 devices
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 *
 * @}
 */

#ifdef MODULE_TMP006

#include "log.h"
#include "saul_reg.h"

#include "tmp006_params.h"

/**
 * @brief   Define the number of configured sensors
 */
#define TMP006_NUM    (sizeof(tmp006_params)/sizeof(tmp006_params[0]))

/**
 * @brief   Allocate memory for the device descriptors
 */
static tmp006_t tmp006_devs[TMP006_NUM];

/**
 * @brief   Memory for the SAUL registry entries
 */
static saul_reg_t saul_entries[TMP006_NUM];

/**
 * @brief   Reference the driver struct
 * @{
 */
extern saul_driver_t tmp006_saul_ambtemp_driver;
/** @} */


void auto_init_tmp006(void)
{
    for (unsigned i = 0; i < TMP006_NUM; i++) {
        LOG_DEBUG("[auto_init_saul] initializing tmp006 #%u\n", i);

        int res = tmp006_init(&tmp006_devs[i], &tmp006_params[i]);
        if (res != 0) {
            LOG_ERROR("[auto_init_saul] error initializing tmp006 #%u\n", i);
        }
        else {
            saul_entries[i].dev = &(tmp006_devs[i]);
            saul_entries[i].name = tmp006_saul_info[i].name;
            saul_entries[i].driver = &tmp006_saul_ambtemp_driver;
            saul_reg_add(&(saul_entries[i]));
        }
    }
}

#else
typedef int dont_be_pedantic;
#endif /* MODULE_TMP006 */
