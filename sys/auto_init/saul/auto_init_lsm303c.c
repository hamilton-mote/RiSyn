/*
 * Copyright (C) 2015 Freie Universität Berlin
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
 * @brief       Auto initialization of LSM303C accelerometer/magnetometer
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#ifdef MODULE_LSM303C

#include "log.h"
#include "saul_reg.h"
#include "lsm303c.h"
#include "lsm303c_params.h"

/**
 * @brief   Define the number of configured sensors
 */
#define LSM303C_NUM    (sizeof(lsm303c_params)/sizeof(lsm303c_params[0]))

/**
 * @brief   Allocate memory for the device descriptors
 */
static lsm303c_t lsm303c_devs[LSM303C_NUM];

/**
 * @brief   Memory for the SAUL registry entries
 */
static saul_reg_t saul_entries[LSM303C_NUM * 2];

/**
 * @brief   Reference the driver structs
 * @{
 */
extern saul_driver_t lsm303c_saul_acc_driver;
extern saul_driver_t lsm303c_saul_mag_driver;
/** @} */

void auto_init_lsm303c(void)
{
    for (unsigned int i = 0; i < LSM303C_NUM; i++) {
        const lsm303c_params_t *p = &lsm303c_params[i];

        LOG_DEBUG("[auto_init_saul] initializing lsm303c #%u\n", i);

        int res = lsm303c_init(&lsm303c_devs[i], p->i2c,
                                  p->acc_addr, p->acc_rate, p->acc_scale,
                                  p->mag_addr, p->mag_rate);
        if (res < 0) {
            LOG_ERROR("[auto_init_saul] error initializing lsm303c #%u\n", i);
            continue;
        }

        saul_entries[(i * 2)].dev = &(lsm303c_devs[i]);
        saul_entries[(i * 2)].name = lsm303c_saul_info[i].name;
        saul_entries[(i * 2)].driver = &lsm303c_saul_acc_driver;
        saul_entries[(i * 2) + 1].dev = &(lsm303c_devs[i]);
        saul_entries[(i * 2) + 1].name = lsm303c_saul_info[i].name;
        saul_entries[(i * 2) + 1].driver = &lsm303c_saul_mag_driver;
        saul_reg_add(&(saul_entries[(i * 2)]));
        saul_reg_add(&(saul_entries[(i * 2) + 1]));
    }
}

#else
typedef int dont_be_pedantic;
#endif /* MODULE_LSM303C */
