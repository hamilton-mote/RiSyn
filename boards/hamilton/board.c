/*
 * Copyright (C) 2016 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     boards_hamilton
 * @{
 *
 * @file
 * @brief       Board specific implementations for the Hamilton mote
 *
 * @author      Michael Andersen <m.andersen@berkeley.edu>
 *
 * @}
 */

#include <stdio.h>

#include "board.h"
#include "cpu.h"

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
    /* initialize the CPU */
    cpu_init();
    /* initialize the boards LED at pin PA19 */
    LED_PORT.DIRSET.reg = (1 << LED_PIN);
    LED_PORT.OUTCLR.reg = (1 << LED_PIN);
    LED_PORT.PINCFG[LED_PIN].bit.PULLEN = false;
}
