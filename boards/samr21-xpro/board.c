/*
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     boards_samr21-xpro
 * @{
 *
 * @file
 * @brief       Board specific implementations for the Atmel SAM R21 Xplained
 *              Pro board
 *
 * @author      Thomas Eichinger <thomas.eichinger@fu-berlin.de>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include "board.h"
#include "periph/gpio.h"

const uint64_t* const fb_sentinel     = ((const uint64_t* const)0x3fc00);
const uint64_t* const fb_flashed_time = ((const uint64_t* const)0x3fc08);
const uint8_t*  const fb_eui64        = ((const uint8_t*  const)0x3fc10);
const uint16_t* const fb_device_id    = ((const uint16_t* const)0x3fc18);
const uint64_t* const fb_designator   = ((const uint64_t* const)0x3fc1c);
const uint8_t*  const fb_aes128_key   = ((const uint8_t*  const)0x3fc30);
const uint8_t*  const fb_25519_pub    = ((const uint8_t*  const)0x3fc40);
const uint8_t*  const fb_25519_priv   = ((const uint8_t*  const)0x3fc60);

void board_init(void)
{
    /* initialize the on-board LED */
    gpio_init(LED0_PIN, GPIO_OUT);

    /* initialize the CPU */
    cpu_init();
}
