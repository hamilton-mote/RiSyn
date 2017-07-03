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
 * @brief       Auto initialization for EKMB1101111 devices
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 *
 * @}
 */

#ifdef MODULE_EKMB1101111

#include "log.h"
#include "saul_reg.h"
#include "ekmb1101111_params.h"

/**
 * @brief   Define the number of configured sensors
 */
#define EKMB1101111_NUM    (sizeof(ekmb1101111_params)/sizeof(ekmb1101111_params[0]))

/**
 * @brief   Allocate memory for the device descriptors
 */
static ekmb1101111_t ekmb1101111_devs[EKMB1101111_NUM];

/**
 * @brief   Memory for the SAUL registry entries
 */
static saul_reg_t saul_entries[EKMB1101111_NUM];

/**
 * @brief   Reference the driver struct
 * @{
 */
extern saul_driver_t ekmb1101111_saul_occup_driver;
/** @} */


void auto_init_ekmb1101111(void)
{
    for (unsigned i = 0; i < EKMB1101111_NUM; i++) {
        LOG_DEBUG("[auto_init_saul] initializing ekmb1101111 #%u\n", i);

        int res = ekmb1101111_init(&ekmb1101111_devs[i], &ekmb1101111_params[i]);
        if (res != 0) {
            LOG_ERROR("[auto_init_saul] error initializing ekmb1101111 #%u\n", i);
        }
        else {
            saul_entries[i].dev = &(ekmb1101111_devs[i]);
            saul_entries[i].name = ekmb1101111_saul_info[i].name;
            saul_entries[i].driver = &ekmb1101111_saul_occup_driver;
            saul_reg_add(&(saul_entries[i]));
        }
    }
}

#else
typedef int dont_be_pedantic;
#endif /* MODULE_EKMB1101111 */
