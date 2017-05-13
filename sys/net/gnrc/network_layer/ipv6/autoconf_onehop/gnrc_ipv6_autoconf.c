/*
 * Copyright (C) 2017 Sam Kumar <samkumar@berkeley.edu>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 */

#include <errno.h>
#include <string.h>

#include "net/gnrc/ipv6.h"
#include "net/ipv6/addr.h"
#include "net/gnrc/ipv6/autoconf_onehop.h"
#include "net/gnrc/ipv6/netif.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#if ENABLE_DEBUG
/* For PRIu8 etc. */
#include <inttypes.h>

static char addr_str[IPV6_ADDR_MAX_STR_LEN];
#endif

static inline void _revert_iid(uint8_t* iid) {
    iid[0] ^= 0x02;
}

kernel_pid_t get_6lowpan_pid(void) {
    kernel_pid_t ifs[GNRC_NETIF_NUMOF];
    size_t ifnum = gnrc_netif_get(ifs);
    for (unsigned i = 0; i < ifnum; i++) {
        gnrc_ipv6_netif_t *ipv6_if = gnrc_ipv6_netif_get(ifs[i]);
        if ((ipv6_if != NULL) && (ipv6_if->flags & GNRC_IPV6_NETIF_FLAGS_SIXLOWPAN)) {
            /* always take the first 6LoWPAN interface we can find */
            return ipv6_if->pid;
        }
    }
    return KERNEL_PID_UNDEF;
}

/* ipv6addr should already have the top 64 bits for the prefix set. */
int gnrc_ipv6_autoconf_l2addr_to_ipv6(ipv6_addr_t* ipv6addr, eui64_t* l2addr) {
    memcpy(&ipv6addr->u8[8], l2addr, sizeof(eui64_t));
    _revert_iid(&ipv6addr->u8[8]);
    return 0;
}

void gnrc_ipv6_autoconf_ipv6_to_l2addr(eui64_t* l2addr, ipv6_addr_t* ipv6addr) {
    memcpy(l2addr, &ipv6addr->u8[8], sizeof(eui64_t));
    _revert_iid((uint8_t*) l2addr);
}

kernel_pid_t gnrc_ipv6_autoconf_next_hop_l2addr(uint8_t* l2addr, uint8_t* l2addr_len, kernel_pid_t iface, ipv6_addr_t *dst) {
    static kernel_pid_t sixlowpan_pid = KERNEL_PID_UNDEF;
    if (sixlowpan_pid == KERNEL_PID_UNDEF) {
        sixlowpan_pid = get_6lowpan_pid();
    }


    if (ipv6_addr_is_link_local(dst)) {
        *l2addr_len = sizeof(eui64_t);
        gnrc_ipv6_autoconf_ipv6_to_l2addr((eui64_t*) l2addr, dst);
        return sixlowpan_pid;
    }

#ifdef I_AM_HAMILTON_BORDER_ROUTER

    ipv6_addr_t* longest_prefix_match;
    kernel_pid_t matching_iface_pid = gnrc_ipv6_netif_find_by_prefix(&longest_prefix_match, dst);
    if (matching_iface_pid == KERNEL_PID_UNDEF) {
        return KERNEL_PID_UNDEF;
    }

    /* I expect the interface to always be the 6LoWPAN interface... */

    uint8_t prefix_length_bits = ipv6_addr_match_prefix(longest_prefix_match, dst);
    if (prefix_length_bits < 64) {
        return KERNEL_PID_UNDEF;
    }

    *l2addr_len = sizeof(eui64_t);
    gnrc_ipv6_autoconf_ipv6_to_l2addr((eui64_t*) l2addr, dst);

    return matching_iface_pid;

#else

    static ipv6_addr_t border_router_ip;
    static bool filled_border_router_ip = false;

    if (!filled_border_router_ip) {
        ipv6_addr_t* rv = ipv6_addr_from_str(&border_router_ip, HAMILTON_BORDER_ROUTER_ADDRESS);
        if (rv == NULL) {
            printf("The HAMILTON_BORDER_ROUTER_ADDRESS is malformed! Check its definition (probably in the Makefile)?\n");
        }
        assert(rv != NULL);
        filled_border_router_ip = true;
    }

    *l2addr_len = sizeof(eui64_t);
    gnrc_ipv6_autoconf_ipv6_to_l2addr((eui64_t*) l2addr, &border_router_ip);

    return sixlowpan_pid;

#endif
}

/** @} */
