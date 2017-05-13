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
 * @brief       TCP socket allocator for GNRC
 *
 * @author      Sam Kumar <samkumar@berkeley.edu>
 *
 * The code that provides the "raw" API for usage of this TCP module; in other
 * words, the frontend of the TCP module.
 * @}
 */

#include <net/tcp_freebsd.h>
#include <sys/socket.h>
#include "gnrc_tcp_freebsd_internal.h"
#include "lib/lbuf.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#define ENABLE_DEBUG (0)
#include "debug.h"

typedef struct asock {
    connectDone_t connectDone;
    sendDone_t sendDone;
    receiveReady_t receiveReady;
    connectionLost_t connectionLost;
    void* context;
} active_socket_t;

typedef struct psock {
    acceptReady_t acceptReady;
    acceptDone_t acceptDone;
    void* context;
} passive_socket_t;

active_socket_t activesockets[GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS];
passive_socket_t passivesockets[GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS];

/* Bitmasks to keep track of which sockets are allocated. */
uint8_t activemask[1 + ((GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS - 1) >> 3)];
uint8_t passivemask[1 + ((GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS - 1) >> 3)];

void clear_activesocket(active_socket_t* asock)
{
    memset(asock, 0x00, sizeof(active_socket_t));
}

void clear_passivesocket(passive_socket_t* psock)
{
    memset(psock, 0x00, sizeof(passive_socket_t));
}

inline int _is_allocated(uint8_t* mask, int fd)
{
    return mask[fd >> 3] & (1 << (fd & 0x7));
}

inline void _force_alloc(uint8_t* mask, int fd)
{
    mask[fd >> 3] |= (1 << (fd & 0x7));
}

inline void _force_dealloc(uint8_t* mask, int fd)
{
    mask[fd >> 3] &= ~(1 << (fd & 0x7));
}

int alloc_fd(uint8_t* mask, int num_fds, bool (*isvalid)(int) ) {
    int i;
    for (i = 0; i < num_fds; i++) {
        if (!_is_allocated(mask, i) && isvalid(i)) {
            _force_alloc(mask, i);
            return i;
        }
    }
    return -1;
}

bool _always_true(int pi)
{
    return true;
}

bool _active_isclosed(int ai)
{
    return TCPS_CLOSED == asock_getState_impl(ai);
}

bool _active_istimewait(int ai)
{
    int state = asock_getState_impl(ai);
    return TCPS_TIME_WAIT == state || TCPS_CLOSED == state;
}

int alloc_pfd(void)
{
    int pfd = alloc_fd(passivemask, GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS, _always_true);
    return pfd + GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS;
}

int alloc_afd(void)
{
    int afd;
    // First, try to get a socket that's closed.
    afd = alloc_fd(activemask, GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS, _active_isclosed);
    if (afd == -1) {
        // If that failed, try to get a socket in TIME-WAIT, and end the TIME-WAIT early.
        afd = alloc_fd(activemask, GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS, _active_istimewait);
        if (afd != -1) {
            asock_abort_impl(afd);
        }
    }
    return afd;
}

void dealloc_fd(uint8_t* mask, int fd)
{
    assert(_is_allocated(mask, fd));
    _force_dealloc(mask, fd);
}

void dealloc_afd(int afd)
{
    assert(afd < GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS);
    dealloc_fd(activemask, afd);
    clear_activesocket(&activesockets[afd]);
}

void dealloc_pfd(int pfd)
{
    assert(pfd < GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS);
    dealloc_fd(passivemask, pfd);
    clear_passivesocket(&passivesockets[pfd]);
}

int decode_fd(int rawfd, bool* passive) {
    if (rawfd < 0 || rawfd >= GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS + GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS) {
        return -1;
    }
    if (rawfd >= GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS) {
        *passive = true;
        rawfd -= GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS;
        if (!_is_allocated(passivemask, rawfd)) {
            return -1;
        }
    } else {
        *passive = false;
        if (!_is_allocated(activemask, rawfd)) {
            return -1;
        }
    }
    return rawfd;
}

/* External API */

int bsdtcp_active_socket(connectDone_t cd, sendDone_t sd, receiveReady_t rr, connectionLost_t cl, void* ctx)
{
    int fd = alloc_afd();
    if (fd != -1) {
        active_socket_t* asock = &activesockets[fd];
        asock->connectDone = cd;
        asock->sendDone = sd;
        asock->receiveReady = rr;
        asock->connectionLost = cl;
        asock->context = ctx;
    }
    return fd;
}

int bsdtcp_passive_socket(acceptReady_t ar, acceptDone_t ad, void* ctx)
{
    int fd = alloc_pfd();
    int decoded_fd = fd - GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS;
    if (fd != -1) {
        passive_socket_t* psock = &passivesockets[decoded_fd];
        psock->acceptReady = ar;
        psock->acceptDone = ad;
        psock->context = ctx;
    }
    return fd;
}

int bsdtcp_set_ctx(int fd, void* newctx) {
    bool passive;
    fd = decode_fd(fd, &passive);
    if (fd == -1) {
        return EBADF;
    }

    if (passive) {
        passive_socket_t* psock = &passivesockets[fd];
        psock->context = newctx;
    } else {
        active_socket_t* asock = &activesockets[fd];
        asock->context = newctx;
    }

    return 0;
}

int bsdtcp_bind(int fd, uint16_t port)
{
    int rv;
    bool passive;
    fd = decode_fd(fd, &passive);
    if (fd == -1) {
        return EBADF;
    }
    if (passive) {
        rv = psock_bind_impl(fd, port);
        DEBUG("Bound passive socket to port %" PRIu16 "\n", port);
    } else {
        rv = asock_bind_impl(fd, port);
        DEBUG("Bound active socket to port %" PRIu16 "\n", port);
    }
    return rv;
}

int bsdtcp_connect(int fd, struct sockaddr_in6* faddrport, uint8_t* recvbuf, size_t recvbuflen, uint8_t* reassbmp)
{
    bool passive;
    fd = decode_fd(fd, &passive);
    if (fd == -1 || passive) {
        return EBADF;
    }
    return asock_connect_impl(fd, faddrport, recvbuf, recvbuflen, reassbmp);
}

int bsdtcp_listen(int fd)
{
    bool passive;
    fd = decode_fd(fd, &passive);
    if (fd == -1 || !passive) {
        return EBADF;
    }
    return psock_listen_impl(fd);
}

int bsdtcp_send(int fd, struct lbufent* data, int* status)
{
    bool passive;
    fd = decode_fd(fd, &passive);
    if (fd == -1 || passive) {
        return EBADF;
    }
    return asock_send_impl(fd, data, 0, status);
}

int bsdtcp_receive(int fd, uint8_t* buffer, size_t length, size_t* numbytes)
{
    bool passive;
    fd = decode_fd(fd, &passive);
    if (fd == -1 || passive) {
        return EBADF;
    }
    return asock_receive_impl(fd, buffer, length, numbytes);
}

int bsdtcp_shutdown(int fd, int how)
{
    bool passive;
    fd = decode_fd(fd, &passive);
    if (fd == -1 || passive) {
        return EBADF;
    }
    return asock_shutdown_impl(fd, how == SHUT_RD || how == SHUT_RDWR,
                                how == SHUT_WR || how == SHUT_RDWR);
}

int bsdtcp_close(int fd)
{
    bool passive;
    int rv;
    fd = decode_fd(fd, &passive);
    if (fd == -1) {
        return EBADF;
    }
    if (passive) {
        rv = psock_close_impl(fd);
        dealloc_pfd(fd);
    } else {
        rv = asock_shutdown_impl(fd, true, true);
        dealloc_afd(fd);
    }
    return rv;
}

int bsdtcp_abort(int fd)
{
    bool passive;
    fd = decode_fd(fd, &passive);
    if (fd == -1) {
        return EBADF;
    }
    if (passive) {
        return psock_close_impl(fd);
    } else {
        return asock_abort_impl(fd);
    }
}

int bsdtcp_isestablished(int fd)
{
    bool passive;
    fd = decode_fd(fd, &passive);
    if (fd == -1 || passive) {
        return EBADF;
    }
    return TCPS_HAVEESTABLISHED(asock_getState_impl(fd));
}

int bsdtcp_hasrcvdfin(int fd)
{
    bool passive;
    int state;
    fd = decode_fd(fd, &passive);
    if (fd == -1 || passive) {
        return EBADF;
    }
    state = asock_getState_impl(fd);
    return state == TCPS_TIME_WAIT || state == TCPS_CLOSE_WAIT ||
            state == TCPS_LAST_ACK || state == TCPS_CLOSING;
}

int bsdtcp_peerinfo(int fd, struct in6_addr** addrptr, uint16_t** portptr)
{
    bool passive;
    fd = decode_fd(fd, &passive);
    if (fd == -1 || passive) {
        return EBADF;
    }
    asock_getPeerInfo_impl(fd, addrptr, portptr);
    return 0;
}

/* API to the TCP frontend. */
acceptArgs_t event_acceptReady(uint8_t pi)
{
    assert(pi >= 0 && pi < GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS);
    assert(_is_allocated(passivemask, pi));

    passive_socket_t* psock = &passivesockets[pi];

    if (psock->acceptReady != NULL) {
        return psock->acceptReady(pi, psock->context);
    } else {
        acceptArgs_t args = { -1, NULL, 0, NULL };
        return args;
    }
}
bool event_acceptDone(uint8_t pi, struct sockaddr_in6* addr, acceptArgs_t* accepted)
{
    assert(pi >= 0 && pi < GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS);
    assert(_is_allocated(passivemask, pi));

    passive_socket_t* psock = &passivesockets[pi];

    if (psock->acceptDone != NULL) {
        return psock->acceptDone(pi, addr, accepted, psock->context);
    }

    return false;
}

void event_connectDone(uint8_t ai, struct sockaddr_in6* addr)
{
    assert(ai >= 0 && ai < GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS);
    // Doesn't need to be allocated

    active_socket_t* asock = &activesockets[ai];

    if (asock->connectDone != NULL) {
        asock->connectDone(ai, addr, asock->context);
    }
}

void event_receiveReady(uint8_t ai, int gotfin)
{
    assert(ai >= 0 && ai < GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS);
    // Doesn't need to be allocated

    active_socket_t* asock = &activesockets[ai];

    if (asock->receiveReady != NULL) {
        asock->receiveReady(ai, gotfin, asock->context);
    }
}

void event_sendDone(uint8_t ai, uint32_t numentries)
{
    assert(ai >= 0 && ai < GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS);
    // Doesn't need to be allocated

    active_socket_t* asock = &activesockets[ai];

    if (asock->sendDone != NULL) {
        asock->sendDone(ai, numentries, asock->context);
    }
}

void event_connectionLost(acceptArgs_t* lost, uint8_t how)
{
    int ai = lost->asockid;
    assert(ai >= 0 && ai < GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS);
    // Doesn't need t be allocated

    active_socket_t* asock = &activesockets[ai];

    if (asock->connectionLost != NULL) {
        asock->connectionLost(lost, how, asock->context);
    }
}

void gnrc_tcp_freebsd_allocator_init(void) {
    int i;
    for (i = 0; i < GNRC_TCP_FREEBSD_NUM_ACTIVE_SOCKETS; i++) {
        clear_activesocket(&activesockets[i]);
    }
    for (i = 0; i < GNRC_TCP_FREEBSD_NUM_PASSIVE_SOCKETS; i++) {
        clear_passivesocket(&passivesockets[i]);
    }
    memset(activemask, 0x00, sizeof(activemask));
    memset(passivemask, 0x00, sizeof(passivemask));
}
