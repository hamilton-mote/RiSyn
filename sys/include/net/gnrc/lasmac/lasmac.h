/*
 * Copyright (C) 2016 Hyung-Sin Kim
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_gnrc_lasmac 
 * @ingroup     net_gnrc
 * @brief       listen-after-send dutycycling MAC protocol
 *
 * @{
 *
 * @file
 * @brief       Interface definition for the LASMAC protocol
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 */

#ifndef NET_GNRC_LASMAC_LASMAC_H
#define NET_GNRC_LASMAC_LASMAC_H

#include "kernel_types.h"
#include "net/gnrc/netdev.h"

#ifndef LEAF_NODE
#define LEAF_NODE (1) /* 0: Always-on router, 1: Duty-cycling leaf node */
#endif

#ifndef DUTYCYCLE_SLEEP_INTERVAL
#define DUTYCYCLE_SLEEP_INTERVAL 2000000UL /* 1) When it is ZERO, a leaf node does not send beacons
                        						(i.e., extremely low duty-cycle,
                                                        but downlink transmission is disabled)
                                              2) Router and leaf node should have same sleep interval.
             								   Router does not sleep                       												   but uses the value for downlink transmissions */
#endif

#ifndef DUTYCYCLE_WAKEUP_INTERVAL
#define DUTYCYCLE_WAKEUP_INTERVAL  6000UL    /* Don't change it w/o particular reasons */
#endif

/**
 * @brief   Type for @ref msg_t if device updates dutycycle operation
 */
#define GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT (0x1235U)
/**
 * @brief   Type for @ref msg_t if device updates dutycycle operation
 */
#define GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_SND (0x1236U)
/**
 * @brief   Type for @ref msg_t if device updates dutycycle operation
 */
#define GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE (0x1237U)

/**
  * @brief Initialize GNRC netdev handler thread for lasmac
  *
  * @param[in] stack         ptr to preallocated stack buffer
  * @param[in] stacksize     size of stack buffer
  * @param[in] priority      priority of thread
  * @param[in] name          name of thread
  * @param[in] gnrc_netdev2  ptr to netdev2 device to handle in created thread
  *
  * @return pid of created thread
  * @return KERNEL_PID_UNDEF on error
  */
kernel_pid_t gnrc_lasmac_init(char *stack, int stacksize, char priority,
                                const char *name, gnrc_netdev_t *gnrc_netdev);

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_LASMAC_LASMAC_H */
/** @} */
