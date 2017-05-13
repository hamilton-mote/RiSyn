/*
 * Copyright (C) 2016 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_gnrc_tcp_freebsd
 * @{
 *
 * @file
 * @brief       TCP checksum calculation for GNRC
 *
 * @author      Sam Kumar <samkumar@berkeley.edu>
 *
 * Unlike the other files in this directory, this is is not taken from the
 * FreeBSD TCP stack.
 * @}
 */
#include "gnrc_tcp_freebsd_internal.h"
#include "bsdtcp/tcp.h"

#include <errno.h>
#include <stdint.h>

inline uint16_t deref_safe(uint16_t* unaligned) {
    return ((uint16_t) *((uint8_t*) unaligned))
        | (((uint16_t) *(((uint8_t*) unaligned) + 1)) << 8);
}

int gnrc_tcp_calc_csum(const gnrc_pktsnip_t *hdr, const gnrc_pktsnip_t *pseudo_hdr)
{
    if (hdr == NULL || pseudo_hdr == NULL) {
        return -EFAULT;
    } else if (hdr->type != GNRC_NETTYPE_TCP) {
        return -EBADMSG;
    } else if (pseudo_hdr->type != GNRC_NETTYPE_IPV6) {
        return -ENOENT;
    }

    struct tcphdr* th = hdr->data;
    th->th_sum = 0;

    const gnrc_pktsnip_t* snips[3];
    snips[0] = hdr;
    snips[1] = (hdr == NULL) ? NULL : hdr->next;
    snips[2] = NULL;

    uint32_t csum = get_tcp_checksum(pseudo_hdr, snips);
    th->th_sum = csum;

    return 0;
}

static uint16_t _calc_checksum(struct in6_addr* src, struct in6_addr* dest,
                               uint32_t ip6hdr_len, const gnrc_pktsnip_t** snips) {
    uint32_t total;
    uint16_t* current;
    uint16_t* end;
    uint32_t currlen;
    int starthalf; // 1 if the end of the last iovec was not half-word aligned
    struct {
        struct in6_addr srcaddr;
        struct in6_addr destaddr;
        uint32_t tcplen;
        uint8_t reserved0;
        uint8_t reserved1;
        uint8_t reserved2;
        uint8_t protocol;
    } __attribute__((packed, aligned)) pseudoheader;
    memcpy(&pseudoheader.srcaddr, src, sizeof(struct in6_addr));
    memcpy(&pseudoheader.destaddr, dest, sizeof(struct in6_addr));
    pseudoheader.reserved0 = 0;
    pseudoheader.reserved1 = 0;
    pseudoheader.reserved2 = 0;
    pseudoheader.protocol = 6; // TCP
    pseudoheader.tcplen = (uint32_t) htonl(ip6hdr_len);

    total = 0;
    for (current = (uint16_t*) &pseudoheader;
         current < (uint16_t*) (&pseudoheader + 1); current++) {
        total += (uint32_t) *current;
    }

    starthalf = 0;
    for (; *snips != NULL; snips++) {
        current = (uint16_t*) (*snips)->data;
        currlen = (uint32_t) (*snips)->size;
        if (starthalf && currlen > 0) {
            total += ((uint32_t) *((uint8_t*) current)) << 8;
            current = (uint16_t*) (((uint8_t*) current) + 1);
            currlen -= 1;
        }
        if (currlen & 0x1u) {
            // This iovec does not end on a half-word boundary
            end = (uint16_t*) (((uint8_t*) current) + currlen - 1);
            total += *((uint8_t*) end);
            starthalf = 1;
        } else {
            // This iovec ends on a half-word boundary
            end = (uint16_t*) (((uint8_t*) current) + currlen);
            starthalf = 0;
        }
        while (current != end) {
            // read the memory byte by byte, in case iovec isn't word-aligned
            total += deref_safe(current++);
        }
    }

    while (total >> 16) {
        total = (total & 0xFFFF) + (total >> 16);
    }

    return ~((uint16_t) total);
}

uint16_t get_tcp_checksum(const gnrc_pktsnip_t *ip6snip, const gnrc_pktsnip_t** snips)
{
    struct ip6_hdr* ip6 = ip6snip->data;
    return _calc_checksum(&ip6->ip6_src, &ip6->ip6_dst,
                            (uint32_t) htons(ip6->ip6_plen), snips);
}
