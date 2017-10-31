/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/**
 * @ingroup auto_init_gnrc_netif
 * @{
 *
 * @file
 * @brief   Auto initialization for ethernet-over-serial module
 *
 * @author  Kaspar Schleiser <kaspar@schleiser.de>
 */

#if defined(MODULE_RETHOS)

#include "log.h"
#include "debug.h"
#include "rethos.h"
#include "periph/uart.h"
#include "net/gnrc/netdev.h"
#include "net/gnrc/netdev/eth.h"

/**
 * @brief global ethos object, used by uart_stdio
 */
ethos_t rethos;

/**
 * @brief   Define stack parameters for the MAC layer thread
 * @{
 */
#define RETHOS_MAC_STACKSIZE (THREAD_STACKSIZE_DEFAULT + DEBUG_EXTRA_STACKSIZE)
#ifndef RETHOS_MAC_PRIO
#define RETHOS_MAC_PRIO      (GNRC_NETDEV_MAC_PRIO)
#endif

/**
 * @brief   Stacks for the MAC layer threads
 */
static char _rethos_stack[RETHOS_MAC_STACKSIZE];
static gnrc_netdev_t _gnrc_rethos;

void auto_init_rethos(void)
{
    LOG_DEBUG("[auto_init_netif] initializing rethos #0\n");

    /* setup netdev device */
    ethos_params_t p;
    p.uart      = RETHOS_UART;
    p.baudrate  = RETHOS_BAUDRATE;
    p.buf       = NULL;
    p.bufsize   = 0;
    rethos_setup(&rethos, &p);

    /* initialize netdev<->gnrc adapter state */
    gnrc_netdev_eth_init(&_gnrc_rethos, (netdev_t*) &rethos);

    /* start gnrc netdev thread */
    gnrc_netdev_init(_rethos_stack, RETHOS_MAC_STACKSIZE, RETHOS_MAC_PRIO,
                      "gnrc_rethos", &_gnrc_rethos);
}

#else
typedef int dont_be_pedantic;
#endif /* MODULE_ETHOS */
/** @} */
