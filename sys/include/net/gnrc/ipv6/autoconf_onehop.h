/*
 * Copyright (C) 2015 Martine Lenders <mlenders@inf.fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for
 * more details.
 */

/**
 * @defgroup    net_gnrc_ipv6_nc  IPv6 neighbor cache
 * @ingroup     net_gnrc_ipv6
 * @brief       Translates IPv6 addresses to link layer addresses.
 * @{
 *
 * @file
 * @brief       Neighbor cache definitions.
 *
 * @author      Martine Lenders <mlenders@inf.fu-berlin.de>
 */

#ifndef GNRC_IPV6_AUTOCONF_ONEHOP_H
#define GNRC_IPV6_AUTOCONF_ONEHOP_H

#include "net/gnrc/ipv6.h"
#include "net/ipv6/addr.h"
#include "net/gnrc/ipv6/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

kernel_pid_t get_6lowpan_pid(void);

/* ipv6addr should already have the top 64 bits for the prefix set. */
int gnrc_ipv6_autoconf_l2addr_to_ipv6(ipv6_addr_t* ipv6addr, eui64_t* l2addr);

void gnrc_ipv6_autoconf_ipv6_to_l2addr(eui64_t* l2addr, ipv6_addr_t* ipv6addr);

kernel_pid_t gnrc_ipv6_autoconf_next_hop_l2addr(uint8_t *l2addr, uint8_t *l2addr_len, kernel_pid_t iface, ipv6_addr_t *dst);

#ifdef __cplusplus
}
#endif

#endif /* GNRC_IPV6_NC_H */
/**
 * @}
 */
