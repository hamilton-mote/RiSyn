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
 * @brief       Auto initialization for PUSH_BUTTON devices
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 *
 * @}
 */

#ifdef MODULE_PUSH_BUTTON

#include "log.h"
#include "saul_reg.h"
#include "push_button_params.h"

/**
 * @brief   Define the number of configured sensors
 */
#define PUSH_BUTTON_NUM    (sizeof(push_button_params)/sizeof(push_button_params[0]))

/**
 * @brief   Allocate memory for the device descriptors
 */
static push_button_t push_button_devs[PUSH_BUTTON_NUM];

/**
 * @brief   Memory for the SAUL registry entries
 */
static saul_reg_t saul_entries[PUSH_BUTTON_NUM];

/**
 * @brief   Reference the driver struct
 * @{
 */
extern saul_driver_t push_button_saul_driver;
/** @} */


void auto_init_push_button(void)
{
    for (unsigned i = 0; i < PUSH_BUTTON_NUM; i++) {
        LOG_DEBUG("[auto_init_saul] initializing push_button #%u\n", i);

        int res = push_button_init(&push_button_devs[i], &push_button_params[i]);
        if (res != 0) {
            LOG_ERROR("[auto_init_saul] error initializing push_button #%u\n", i);
        }
        else {
            saul_entries[i].dev = &(push_button_devs[i]);
            saul_entries[i].name = push_button_saul_info[i].name;
            saul_entries[i].driver = &push_button_saul_driver;
            saul_reg_add(&(saul_entries[i]));
        }
    }
}

#else
typedef int dont_be_pedantic;
#endif /* MODULE_PUSH_BUTTON */
