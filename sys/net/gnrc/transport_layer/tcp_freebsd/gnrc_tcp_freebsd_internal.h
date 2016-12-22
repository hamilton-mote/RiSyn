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
 * @brief       Internal API to the TCP frontend for GNRC
 *
 * @author      Sam Kumar <samkumar@berkeley.edu>
 *
 * This file describes the API that the TCP frontend presents to the TCP
 * protocol logic. The protocol logic interacts with other parts of the
 * kernel (GNRC, xtimer, etc.) via this API. It also describes the API that
 * the TCP frontend presents to the interface to the GNRC.
 * @}
 */

#ifndef GNRC_TCP_FREEBSD_INTERNAL_H_
#define GNRC_TCP_FREEBSD_INTERNAL_H_

#include <errno.h>
#include <stdio.h>
#include "bsdtcp/ip6.h"
#include "bsdtcp/tcp.h"
#include "bsdtcp/tcp_fsm.h"
#include "bsdtcp/tcp_timer.h"
#include "bsdtcp/tcp_var.h"
#include "net/gnrc/pkt.h"
#include "net/tcp_freebsd.h"

#define GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS 1
#define GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS 1

#define TIMERS_PER_ACTIVE_SOCKET 4

/* Possible return value from tcp_input. */
#define RELOOKUP_REQUIRED -1

#define IANA_TCP PROTNUM_TCP

#define hz 1000 // number of ticks per second
#define MICROS_PER_TICK 1000 // number of microseconds per tick

#define FRAMES_PER_SEG 3
#define FRAMECAP_6LOWPAN (122 - 22 - 12) // Fragmentation limit: maximum frame size of the IP and TCP headers

#define COMPRESSED_IP6HDR_SIZE (2 + 1 + 1 + 16 + 8) // IPHC header (2) + Next header (1) + Hop count (1) + Dest. addr (16) + Src. addr (8)

#define SIG_CONN_ESTABLISHED 0x01
#define SIG_RECVBUF_NOTEMPTY 0x02
#define SIG_RCVD_FIN         0x04

#define GNRC_TCP_FREEBSD_NUM_TIMERS (GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS * TIMERS_PER_ACTIVE_SOCKET)

#define CONN_LOST_NORMAL 0 // errno of 0 means that the connection closed gracefully

struct ip6_packet {
    // Dummy for now
    struct ip6_hdr ip6_hdr;
    struct ip_iovec* ip6_data;
};

/*
 * Functions that the TCP protocol logic can call to interact with the rest of
 * the kernel.
 */
void send_message(gnrc_pktsnip_t* pkt);
uint32_t get_ticks(void);
uint32_t get_millis(void);
void set_timer(struct tcpcb* tcb, uint8_t timer_id, uint32_t delay);
void stop_timer(struct tcpcb* tcb, uint8_t timer_id);
struct tcpcb* accept_ready(struct tcpcb_listen* tpl);
bool accepted_connection(struct tcpcb_listen* tpl, struct tcpcb* accepted, struct in6_addr* addr, uint16_t port);
void connection_lost(struct tcpcb* tcb, uint8_t errnum);
uint16_t get_tcp_checksum(gnrc_pktsnip_t *ip6snip, gnrc_pktsnip_t** snips);

/*
 * Functions that the TCP API code can call to interact with the rest of the
 * TCP stack.
 */
int psock_getID_impl(int psockid);
int asock_getID_impl(int asockid);
int asock_getState_impl(int asockid);
void asock_getPeerInfo_impl(int asockid, struct in6_addr** addr, uint16_t** port);
error_t asock_bind_impl(int asockid, uint16_t port);
error_t psock_bind_impl(int psockid, uint16_t port);
error_t psock_listen_impl(int psockid);
error_t asock_connect_impl(int asockid, struct sockaddr_in6* addr, uint8_t* recvbuf, size_t recvbuflen, uint8_t* reassbmp);
error_t asock_send_impl(int asockid, struct lbufent* data, int moretocome, int* status);
error_t asock_receive_impl(int asockid, uint8_t* buffer, uint32_t len, size_t* bytessent);
error_t asock_shutdown_impl(int asockid, bool shut_rd, bool shut_wr);
error_t psock_close_impl(int psockid);
error_t asock_abort_impl(int asockid);

/*
 * Functions that allow the TCP protocol logic to inform the user of TCP-related
 * events.
 */
void gnrc_tcp_freebsd_allocator_init(void);
acceptArgs_t event_acceptReady(uint8_t pi);
bool event_acceptDone(uint8_t pi, struct sockaddr_in6* addr, int asockid);
void event_connectDone(uint8_t ai, struct sockaddr_in6* addr);
void event_receiveReady(uint8_t ai, int gotfin);
void event_sendDone(uint8_t ai, uint32_t numentries);
void event_connectionLost(uint8_t ai, uint8_t how);

#endif // GNRC_TCP_FREEBSD_INTERNAL_H_
