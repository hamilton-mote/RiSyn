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
 * @brief       TCP interface to the GNRC
 *
 * @author      Sam Kumar <samkumar@berkeley.edu>
 *
 * Based partially on sys/net/gnrc/transport_layer/udp/gnrc_udp.c.
 * @}
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include "gnrc_tcp_freebsd_internal.h"

#include "msg.h"
#include "thread.h"
#include "net/gnrc/pkt.h"
#include "net/gnrc/tcp_freebsd.h"

#include "bsdtcp/tcp.h"
#include "bsdtcp/tcp_fsm.h"
#include "bsdtcp/tcp_var.h"

#include "task_sched.h"
#include "xtimer.h"

#include "mutex.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#define SUCCESS 0
 /**
  * @brief   Save the TCP thread IDs for later reference (just like the UDP
  *          implementation)
  */
static kernel_pid_t _packet_pid = KERNEL_PID_UNDEF;
static kernel_pid_t _timer_pid = KERNEL_PID_UNDEF;

static mutex_t tcp_lock = MUTEX_INIT;

/**
 * @brief    Statically allocated pools of active and passive TCP sockets
 */
struct tcpcb tcbs[GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS];
struct tcpcb_listen tcbls[GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS];

/**
 * @brief    Timers used for TCP. Each active socket requires four timers.
 */
static struct task_sched tcp_timer_sched;
struct task tcp_timers[GNRC_TCP_FREEBSD_NUM_TIMERS];

/**
 * @brief   Allocate memory for the TCP thread's stack
 */
#if ENABLE_DEBUG
static char _packet_stack[GNRC_TCP_FREEBSD_STACK_SIZE + THREAD_EXTRA_STACKSIZE_PRINTF];
static char _timer_stack[GNRC_TCP_FREEBSD_STACK_SIZE + THREAD_EXTRA_STACKSIZE_PRINTF];
#else
static char _packet_stack[GNRC_TCP_FREEBSD_STACK_SIZE];
static char _timer_stack[GNRC_TCP_FREEBSD_STACK_SIZE];
#endif

static void _handle_timer(int timer_id)
{
    struct tcpcb* tp;
    DEBUG("Timer %d fired!\n", timer_id);
    assert((timer_id >> 2) < GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS);

    tp = &tcbs[timer_id >> 2];
    timer_id &= 0x3;

    mutex_lock(&tcp_lock);

    switch (timer_id) {
    case TOS_DELACK:
        DEBUG("Delayed ACK\n");
        tcp_timer_delack(tp);
        break;
    case TOS_REXMT: // Also include persist case
        if (tcp_timer_active(tp, TT_REXMT)) {
            DEBUG("Retransmit\n");
            tcp_timer_rexmt(tp);
        } else {
            DEBUG("Persist\n");
            tcp_timer_persist(tp);
        }
        break;
    case TOS_KEEP:
        DEBUG("Keep\n");
        tcp_timer_keep(tp);
        break;
    case TOS_2MSL:
        DEBUG("2MSL\n");
        tcp_timer_2msl(tp);
        break;
    }

    mutex_unlock(&tcp_lock);
}

/**
 *  @brief   Passes signals to the user of this module.
 */
void handle_signals(struct tcpcb* tp, uint8_t signals, uint32_t freedentries)
{
    struct sockaddr_in6 addrport;

    if (signals & SIG_CONN_ESTABLISHED && !tpispassiveopen(tp)) {
        addrport.sin6_port = tp->fport;
        memcpy(&addrport.sin6_addr, &tp->faddr, sizeof(addrport.sin6_addr));

        event_connectDone((uint8_t) tp->index, &addrport);
    }

    if (signals & SIG_RECVBUF_NOTEMPTY) {
        event_receiveReady((uint8_t) tp->index, 0);
    }

    if (signals & SIG_RCVD_FIN) {
        event_receiveReady((uint8_t) tp->index, 1);
    }

    if (freedentries > 0) {
        event_sendDone((uint8_t) tp->index, freedentries);
    }
}

/**
 * Called when an active socket loses a connection.
 */
void connection_lost(struct tcpcb* tcb, uint8_t errnum)
{
    mutex_unlock(&tcp_lock);
    event_connectionLost((uint8_t) tcb->index, errnum);
    mutex_lock(&tcp_lock);
}

/**
 * Called when a passive socket is about to accept a connection,
 * and needs an active socket to accept into.
 */
struct tcpcb* accept_ready(struct tcpcb_listen* tpl)
{
    acceptArgs_t args;
    mutex_unlock(&tcp_lock);
    args = event_acceptReady((uint8_t) tpl->index);
    mutex_lock(&tcp_lock);
    if (args.asockid == -1) {
        return NULL;
    }
    assert(args.asockid >= 0 && args.asockid < GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS);
    struct tcpcb* asock = &tcbs[args.asockid];
    initialize_tcb(asock, asock->lport, args.recvbuf, args.recvbuflen, args.reassbmp);
    return asock;
}

/**
 * Called when a passive socket accepts a connection.
 */
bool accepted_connection(struct tcpcb_listen* tpl, struct tcpcb* accepted, struct in6_addr* addr, uint16_t port)
{
    bool accepted_successfully;
    struct sockaddr_in6 addrport;
    mutex_unlock(&tcp_lock);
    addrport.sin6_port = port;
    memcpy(&addrport.sin6_addr, addr, sizeof(struct in6_addr));
    accepted_successfully = event_acceptDone((uint8_t) tpl->index, &addrport, accepted->index);
    tpl->t_state = TCPS_CLOSED;
    mutex_lock(&tcp_lock);

    return accepted_successfully;
}

/**
 * @brief   Called when a TCP segment is received and passed up from the IPv6
 *          layer.
 */
static void _receive(gnrc_pktsnip_t* pkt)
{
    gnrc_pktsnip_t* tcp;
    gnrc_pktsnip_t* ipv6;
    struct tcphdr* th;
    struct tcpcb* tcb;
    struct tcpcb_listen* tcbl;
    struct ip6_hdr* iph;

    int i;
    uint16_t sport;
    uint16_t dport;
    uint16_t packet_len;

    uint16_t empirical_len;
    gnrc_pktsnip_t* temp;

    /* Bitmask of signals that need to be sent to the user of this module. */
    uint8_t signals = 0;

    /* Number of lbuf entries that the user of this module can free. */
    uint32_t freedentries = 0;

    tcp = gnrc_pktbuf_start_write(pkt);
    if (tcp == NULL) {
        DEBUG("tcp_freebsd: unable to get write access to packet\n");
        goto done;
    }
    pkt = tcp;

    ipv6 = gnrc_pktsnip_search_type(pkt, GNRC_NETTYPE_IPV6);
    assert(ipv6 != NULL);

    iph = (struct ip6_hdr*) ipv6->data;

    /* I'm actually not going to mark the TCP section. The tcp_input function
     * is written to consider both the TCP section and the payload together,
     * and specifically making it would just take up extra memory for the new
     * pktsnip.
     */
#if 0
    if ((pkt->next != NULL) && (pkt->next->type == GNRC_NETTYPE_TCP)) {
        /* Someone already marked the TCP header, so we can just use it. */
        tcp = pkt->next;
    } else if (pkt->size >= sizeof(struct tcphdr)) {
        /* The TCP header may include options, and therefore may have variable
         * length. So we need to actually parse it first, in order to correctly
         * mark it...
         */
         th = (struct tcphdr*) pkt->data;
         if (th->th_off < 5 || th->th_off > 15) {
             goto error;
         }

         /* This is the size of the TCP header, in bytes. */
         size_t hdrlen = ((size_t) th->th_off) << 2;

         tcp = gnrc_pktbuf_mark(pkt, hdrlen, GNRC_NETTYPE_TCP);
         if (tcp == NULL) {
             DEBUG("tcp_freebsd: error marking TCP header, dropping packet\n");
             goto error;
         }
    } else {
        goto error;
    }

    /* Mark payload as type UNDEF. */
    pkt->type = GNRC_NETTYPE_UNDEF;
#endif

    /*
     * If someone is splitting this for us, that's a problem, since the TCP
     * header and the payload need to be contiguous, in one big snip. So I'm
     * asserting that the topmost identified layer in the packet is indeed
     * labelled as a TCP header.
     */
    assert(tcp->type == GNRC_NETTYPE_TCP);

    th = (struct tcphdr*) tcp->data;

    packet_len = htons(iph->ip6_plen);
    empirical_len = 0;
    for (temp = tcp; temp != ipv6; temp = temp->next) {
        DEBUG("Size is %" PRIu16 "\n", temp->size);
        empirical_len += temp->size;
    }

    if (packet_len != empirical_len) {
        DEBUG("Sizes don't add up: packet length is %" PRIu16 ", but got %" PRIu16 "\n", packet_len, empirical_len);
        goto done;
    }
    if (th->th_off < 5 || th->th_off > 15 || (((size_t) th->th_off) << 2) > tcp->size) {
        DEBUG("Too many options: header claims %" PRIu8 " words (pktsnip has %u bytes)\n", th->th_off, (unsigned int) tcp->size);
    }

    gnrc_pktsnip_t* snips[2] = { tcp, NULL };
    uint16_t csum = get_tcp_checksum(ipv6, snips);
    if (csum != 0) {
        DEBUG("Dropping packet: bad checksum (%" PRIu16 ")\n", csum);
        goto done;
    }

    sport = th->th_sport; // network byte order
    dport = th->th_dport; // network byte order
    tcp_fields_to_host(th);

    /* Actually do the work. */
    for (i = 0; i < GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS; i++) {
        tcb = &tcbs[i];
        if (tcb->t_state != TCP6S_CLOSED && dport == tcb->lport
            && sport == tcb->fport
            && !memcmp(&iph->ip6_src, &tcb->faddr, sizeof(iph->ip6_src))) {
            int rv;
            DEBUG("Matches active socket %d\n", i);
            mutex_lock(&tcp_lock);
            rv = tcp_input(iph, th, &tcbs[i], NULL, &signals, &freedentries);
            mutex_unlock(&tcp_lock);
            if (RELOOKUP_REQUIRED == rv) {
                break;
            } else {
                handle_signals(&tcbs[i], signals, freedentries);
            }
            goto done;
        }
    }

    for (i = 0; i < GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS; i++) {
        tcbl = &tcbls[i];
        if (tcbl->t_state == TCP6S_LISTEN && dport == tcbl->lport) {
            DEBUG("Matches passive socket %d\n", i);
            mutex_lock(&tcp_lock);
            tcp_input(iph, th, NULL, &tcbls[i], NULL, NULL);
            mutex_unlock(&tcp_lock);
            goto done;
        }
    }

    DEBUG("Does not match any socket\n");
    tcp_dropwithreset(iph, th, NULL, tcp->size - (th->th_off << 2), ECONNREFUSED);

done:
    gnrc_pktbuf_release(pkt);
    return;
}

/**
 * @brief Event loop for received TCP segments.
 */
static void* _packet_loop(void* arg)
{
    (void) arg;
    msg_t msg;
    msg_t setget_reply;
    msg_t msg_queue[GNRC_TCP_FREEBSD_MSG_QUEUE_SIZE];

    /* _packet_pid may not be assigned, if the scheduler switches to this thread
     * before thread_create returns, or after thread_create returns but before
     * the return value is stored.
     */
    _packet_pid = thread_getpid();

    gnrc_netreg_entry_t netreg = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL,
                                                            _packet_pid);

    setget_reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
    setget_reply.content.value = (uint32_t) -ENOTSUP;

    msg_init_queue(msg_queue, GNRC_TCP_FREEBSD_MSG_QUEUE_SIZE);

    if (gnrc_netreg_register(GNRC_NETTYPE_TCP, &netreg)) {
        DEBUG("Error listening for packets\n");
    }

    for (;;) {
        msg_receive(&msg);
        switch (msg.type) {
            case GNRC_NETAPI_MSG_TYPE_RCV:
                DEBUG("tcp_freebsd: got RCV message: %p\n", msg.content.ptr);
                _receive(msg.content.ptr);
                break;
            case GNRC_NETAPI_MSG_TYPE_SND:
                /* Not sure what kind of protocol is going to pass a packet
                 * down to TCP, since the whole point of TCP is that protocols
                 * on top of it deal with _streams_ rather than _packets_.
                 */
                DEBUG("tcp_freebsd: got SND message: %p\n", msg.content.ptr);
                break;
            case GNRC_NETAPI_MSG_TYPE_SET:
            case GNRC_NETAPI_MSG_TYPE_GET:
                msg_reply(&msg, &setget_reply);
            case GNRC_NETAPI_MSG_TYPE_ACK:
                DEBUG("tcp_freebsd: received SET, GET, or ACK\n");
                break;
            default:
                DEBUG("tcp_freebsd: received unidentified message\n");
                break;
        }
    }

    /* not reached */
    return NULL;
}

int gnrc_tcp_freebsd_init(void)
{
    int i;

    gnrc_tcp_freebsd_allocator_init();

    if (_packet_pid == KERNEL_PID_UNDEF) {
        _packet_pid = thread_create(_packet_stack, sizeof(_packet_stack),
                             GNRC_TCP_FREEBSD_PRIO, THREAD_CREATE_STACKTEST,
                             _packet_loop, NULL, "tcp_freebsd");
        tcp_timer_sched.coalesce_shift = 64;
        tcp_timer_sched.max_coalesce_time_delta = 0; // no coalescence for now
        tcp_timer_sched.tasks = tcp_timers;
        tcp_timer_sched.num_tasks = GNRC_TCP_FREEBSD_NUM_TIMERS;
        tcp_timer_sched.thread_stack = _timer_stack;
        tcp_timer_sched.thread_stack_size = sizeof(_timer_stack);
        tcp_timer_sched.thread_priority = GNRC_TCP_FREEBSD_PRIO;
        tcp_timer_sched.thread_name = "tcp_freebsd timers";
        tcp_timer_sched.task_handler = _handle_timer;
        _timer_pid = start_task_sched(&tcp_timer_sched);

        /* Additional initialization work for TCP. */
        tcp_init();
        for (i = 0; i < GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS; i++) {
            tcbs[i].index = i;
            initialize_tcb(&tcbs[i], 0, NULL, 0, NULL);
        }
        for (i = 0; i < GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS; i++) {
            tcbls[i].t_state = TCPS_CLOSED;
            tcbls[i].index = i;
            tcbls[i].lport = 0;
        }
    }
    return _packet_pid;
}

/* A helper function. PORT is in network byte order. */
bool portisfree(uint16_t port)
{
    int i;
    for (i = 0; i < GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS; i++) {
        if (tcbs[i].lport == port) {
            return false;
        }
    }
    for (i = 0; i < GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS; i++) {
        if (tcbls[i].lport == port) {
            return false;
        }
    }
    return true;
}

/* The external API. */

int psock_getID_impl(int psockid)
{
    return tcbls[psockid].index;
}

int asock_getID_impl(int asockid)
{
    return tcbs[asockid].index;
}

int asock_getState_impl(int asockid)
{
    return tcbs[asockid].t_state;
}

void asock_getPeerInfo_impl(int asockid, struct in6_addr** addr, uint16_t** port)
{
    mutex_lock(&tcp_lock);
    *addr = &tcbs[asockid].faddr;
    *port = &tcbs[asockid].fport;
    mutex_unlock(&tcp_lock);
}

error_t asock_bind_impl(int asockid, uint16_t port)
{
    error_t rv;
    uint16_t oldport;
    mutex_lock(&tcp_lock);
    oldport = tcbs[asockid].lport;
    port = htons(port);
    tcbs[asockid].lport = 0;
    if (port == 0 || portisfree(port)) {
        tcbs[asockid].lport = port;
        rv = SUCCESS;
        goto done;
    }
    tcbs[asockid].lport = oldport;
    rv = EADDRINUSE;
done:
    mutex_unlock(&tcp_lock);
    return rv;
}

error_t psock_bind_impl(int psockid, uint16_t port)
{
    error_t rv;
    uint16_t oldport;
    mutex_lock(&tcp_lock);
    oldport = tcbls[psockid].lport;
    port = htons(port);
    tcbls[psockid].lport = 0;
    if (port == 0 || portisfree(port)) {
        tcbls[psockid].lport = port;
        rv = SUCCESS;
        goto done;
    }
    tcbls[psockid].lport = oldport;
    rv = EADDRINUSE;
done:
    mutex_unlock(&tcp_lock);
    return rv;
}

error_t psock_listen_impl(int psockid)
{
    mutex_lock(&tcp_lock);
    tcbls[psockid].t_state = TCPS_LISTEN;
    mutex_unlock(&tcp_lock);
    return SUCCESS;
}

error_t asock_connect_impl(int asockid, struct sockaddr_in6* addr, uint8_t* recvbuf, size_t recvbuflen, uint8_t* reassbmp)
{
    error_t rv;
    struct tcpcb* tp = &tcbs[asockid];
    mutex_lock(&tcp_lock);
    if (tp->t_state != TCPS_CLOSED) { // This is a check that I added
        rv = EISCONN;
        goto done;
    }
    initialize_tcb(tp, tp->lport, recvbuf, recvbuflen, reassbmp);
    rv = (error_t) tcp6_usr_connect(tp, addr);

done:
    mutex_unlock(&tcp_lock);
    return rv;
}

error_t asock_send_impl(int asockid, struct lbufent* data, int moretocome, int* status)
{
    error_t rv;
    struct tcpcb* tp = &tcbs[asockid];
    mutex_lock(&tcp_lock);
    rv = (error_t) tcp_usr_send(tp, moretocome, data, status);
    mutex_unlock(&tcp_lock);
    return rv;
}

error_t asock_receive_impl(int asockid, uint8_t* buffer, uint32_t len, size_t* bytessent)
{
    error_t rv;
    struct tcpcb* tp = &tcbs[asockid];
    mutex_lock(&tcp_lock);
    *bytessent = cbuf_read(&tp->recvbuf, buffer, len, 1);
    rv = (error_t) tcp_usr_rcvd(tp);
    mutex_unlock(&tcp_lock);
    return rv;
}

error_t asock_shutdown_impl(int asockid, bool shut_rd, bool shut_wr)
{
    int error = SUCCESS;
    mutex_lock(&tcp_lock);
    if (shut_rd) {
        cbuf_pop(&tcbs[asockid].recvbuf, cbuf_used_space(&tcbs[asockid].recvbuf)); // remove all data from the cbuf
        // TODO We need to deal with bytes received out-of-order
        // Our strategy is to "pretend" that we got those extra bytes and ACK them.
        tpcantrcvmore(&tcbs[asockid]);
    }
    if (shut_wr) {
        error = tcp_usr_shutdown(&tcbs[asockid]);
    }
    mutex_unlock(&tcp_lock);
    return error;
}

error_t psock_close_impl(int psockid)
{
    mutex_lock(&tcp_lock);
    tcbls[psockid].t_state = TCP6S_CLOSED;
    mutex_unlock(&tcp_lock);
    return SUCCESS;
}

error_t asock_abort_impl(int asockid)
{
    mutex_lock(&tcp_lock);
    tcp_usr_abort(&tcbs[asockid]);
    mutex_unlock(&tcp_lock);
    return SUCCESS;
}

/* The internal API. */

static int ctr = 0;
void send_message(gnrc_pktsnip_t* pkt)
{
    if (ctr++) {
        //return;
    }
    DEBUG("Sending TCP message: %d\n", pkt->type);
    if (!gnrc_netapi_dispatch_send(pkt->type, GNRC_NETREG_DEMUX_CTX_ALL, pkt)) {
        DEBUG("tcp: cannot send packet: network layer not found\n");
        gnrc_pktbuf_release(pkt);
    }
}

uint32_t get_millis(void)
{
    uint64_t micros = xtimer_now_usec64();
    return micros / 1000;
}

uint32_t get_ticks(void)
{
    return get_millis();
}


/*
 * The lock ordering for the timing code is that the TCP lock is always
 * acquired first, and then the timer lock.
 */
void set_timer(struct tcpcb* tcb, uint8_t timer_id, uint32_t delay)
{
    int task_id = (((int) tcb->index) << 2) | (int) timer_id;
    int64_t delay_micros = MICROS_PER_TICK * (int64_t) delay;

    DEBUG("Setting timer %d: %d\n", task_id, (int) (delay_micros / 1000));

    if (sched_task(&tcp_timer_sched, task_id, delay_micros) != 0) {
        DEBUG("sched_task failed!\n");
    }
}

void stop_timer(struct tcpcb* tcb, uint8_t timer_id)
{
    int task_id = (((int) tcb->index) << 2) | (int) timer_id;

    DEBUG("Stopping timer %d\n", task_id);

    if (cancel_task(&tcp_timer_sched, task_id) != 0) {
        DEBUG("cancel_task failed!\n");
    }
}
