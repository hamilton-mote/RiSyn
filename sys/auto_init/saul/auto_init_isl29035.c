/*
 * Copyright (C) 2018 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */


/*
 * @ingroup     auto_init_saul
 * @{
 *
 * @file
 * @brief       Auto initialization of ISL29035 light sensors
 *
 * @author      Michael Andersen <m.andersen@berkeley.edu>
 *
 * @}
 */

#ifdef MODULE_ISL29035

#include "log.h"
#include "saul_reg.h"
#include "isl29035.h"
#include "isl29035_params.h"

/**
 * @brief   Define the number of configured sensors
 */
#define ISL29035_NUM    (sizeof(isl29035_params)/sizeof(isl29035_params[0]))

/**
 * @brief   Allocate memory for the device descriptors
 */
static isl29035_t isl29035_devs[ISL29035_NUM];

/**
 * @brief   Memory for the SAUL registry entries
 */
static saul_reg_t saul_entries[ISL29035_NUM];

/**
 * @brief   Reference the driver struct
 */
extern saul_driver_t isl29035_saul_driver;


void auto_init_isl29035(void)
{
    for (unsigned int i = 0; i < ISL29035_NUM; i++) {
        const isl29035_params_t *p = &isl29035_params[i];

        LOG_DEBUG("[auto_init_saul] initializing isl29035 #%u\n", i);

        int res = isl29035_init(&isl29035_devs[i], p->i2c, p->addr,
                                p->range, p->mode);
        if (res < 0) {
            LOG_ERROR("[auto_init_saul] error initializing isl29035 #%u\n", i);
            continue;
        }

        saul_entries[i].dev = &(isl29035_devs[i]);
        saul_entries[i].name = isl29035_saul_info[i].name;
        saul_entries[i].driver = &isl29035_saul_driver;
        saul_reg_add(&(saul_entries[i]));
    }
}

#else
typedef int dont_be_pedantic;
#endif /* MODULE_ISL29035 */
