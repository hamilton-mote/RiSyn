/*
 * Copyright (C) 2016 University of California, Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_gnrc_tcp_freebsd TCP_FREEBSD
 * @ingroup     net_gnrc
 * @brief       FreeBSD TCP Frontend for GNRC
 *
 * @{
 *
 * @file
 * @brief       TCP GNRC definition
 *
 * @author      Sam Kumar <samkumar@berkeley.edu>
 *
 * This file is largely based on sys/include/net/gnrc/udp.h.
 */

#ifndef GNRC_TCP_FREEBSD_H_
#define GNRC_TCP_FREEBSD_H_

#include <stdint.h>

#include "byteorder.h"
#include "net/gnrc.h"
//#include "net/udp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Default message queue size for the TCP thread
 */
#ifndef GNRC_TCP_FREEBSD_MSG_QUEUE_SIZE
#define GNRC_TCP_FREEBSD_MSG_QUEUE_SIZE (8U)
#endif

/**
 * @brief   Priority of the TCP thread
 */
#ifndef GNRC_TCP_FREEBSD_PRIO
#define GNRC_TCP_FREEBSD_PRIO           (THREAD_PRIORITY_MAIN - 2)
#endif

/**
 * @brief   Default stack size to use for the TCP thread
 */
#ifndef GNRC_TCP_FREEBSD_STACK_SIZE
#define GNRC_TCP_FREEBSD_STACK_SIZE     (THREAD_STACKSIZE_DEFAULT)
#endif

/**
 * @brief   Calculate the TCP checksum for the given packet
 *
 * @param[in] hdr           Pointer to the TCP header
 * @param[in] pseudo_hdr    Pointer to the network layer header
 *
 * @return  0 on success
 * @return  -EBADMSG if @p hdr is not of type GNRC_NETTYPE_TCP
 * @return  -EFAULT if @p hdr or @p pseudo_hdr is NULL
 * @return  -ENOENT if gnrc_pktsnip_t::type of @p pseudo_hdr is not known
 */
int gnrc_tcp_calc_csum(gnrc_pktsnip_t *hdr, gnrc_pktsnip_t *pseudo_hdr);

#if 0
/**
 * @brief   Allocate and initialize a fresh UDP header in the packet buffer
 *
 * @param[in] payload       Payload contained in the UDP packet
 * @param[in] src           Source port in host byte order
 * @param[in] dst           Destination port in host byte order
 *
 * @return  pointer to the newly created (and allocated) header
 * @return  NULL on `src == NULL`, `dst == NULL`, `src_len != 2`, `dst_len != 2`
 *          or on allocation error
 */
gnrc_pktsnip_t *gnrc_udp_hdr_build(gnrc_pktsnip_t *payload, uint16_t src,
                                   uint16_t dst);
#endif

/**
 * @brief   Initialize and start TCP
 *
 * @return  PID of the TCP thread
 * @return  negative value on error
 */
int gnrc_tcp_freebsd_init(void);

#ifdef __cplusplus
}
#endif

#endif /* GNRC_TCP_FREEBSD_H_ */
/** @} */
