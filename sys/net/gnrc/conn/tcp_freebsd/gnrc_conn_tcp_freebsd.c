/**
* This file is subject to the terms and conditions of the GNU Lesser
* General Public License v2.1. See the file LICENSE in the top level
* directory for more details.
*/

/**
 * @{
 *
 * @file
 * @brief       Implementation of conn API for GNRC TCP derived from FreeBSD
 *
 * @author  Sam Kumar <samkumar@berkeley.edu>
 */

#include <errno.h>
#include "net/af.h"
#include "net/gnrc/conn.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/tcp_freebsd.h"
#include "net/conn/tcp_freebsd.h"
#include "net/tcp_freebsd.h"
#include "zone/gnrc_conn_tcp_freebsd_zalloc.h"

#define ENABLE_DEBUG (0)

#include "debug.h"

#define RECV_BUF_LEN 864
#define REASS_BMP_LEN ((RECV_BUF_LEN + 7) >> 3)

#define SENDMAXCOPY 52
#define COPYBUFSIZE (SENDMAXCOPY << 1)
#define SENDBUFSIZE 864

/* Cached copy buffer, which may help us avoid a dynamic memory allocation. */
struct conn_tcp_freebsd_send_state* extracopybuf = NULL;

static uint32_t _free_sendstates(conn_tcp_freebsd_t* conn, uint32_t howmany) {
    uint32_t totalbytesremoved = 0;
    uint32_t i;

    struct conn_tcp_freebsd_send_state* head;
    struct conn_tcp_freebsd_send_state* newhead;
    for (i = 0; (i < howmany) && (conn->sfields.active.send_head != NULL); i++) {
        head = conn->sfields.active.send_head;
        newhead = head->next;
        totalbytesremoved += (head->buflen - head->entry.extraspace);
        if (head->buflen == COPYBUFSIZE && extracopybuf == NULL) {
            /* Hang on to the reference, to avoid a future memory allocation. */
            extracopybuf = head;
        } else {
            conn_tcp_freebsd_zfree(head);
        }
        conn->sfields.active.send_head = newhead;
    }
    if (conn->sfields.active.send_head == NULL) {
        conn->sfields.active.send_tail = NULL;
    }
    assert(totalbytesremoved <= conn->sfields.active.in_send_buffer);
    conn->sfields.active.in_send_buffer -= totalbytesremoved;
    return totalbytesremoved;
}

static void conn_tcp_freebsd_connectDone(uint8_t ai, struct sockaddr_in6* faddr, void* ctx)
{
    assert(ctx != NULL);
    (void) ai;
    (void) faddr;
    conn_tcp_freebsd_t* conn = ctx;
    mutex_lock(&conn->lock);
    assert(conn->hasactive && !conn->haspassive);
    conn->errstat = 0;
    cond_signal(&conn->sfields.active.connect_cond);
    mutex_unlock(&conn->lock);
}

static void conn_tcp_freebsd_sendDone(uint8_t ai, uint32_t tofree, void* ctx)
{
    (void) ai;
    uint32_t freed;

    conn_tcp_freebsd_t* conn = ctx;
    assert(conn != NULL);

    mutex_lock(&conn->lock);
    assert(conn->hasactive && !conn->haspassive);
    freed = _free_sendstates(conn, tofree);
    if (freed > 0) {
        cond_broadcast(&conn->sfields.active.send_cond);
    }
    mutex_unlock(&conn->lock);
}

static void conn_tcp_freebsd_receiveReady(uint8_t ai, int gotfin, void* ctx)
{
    // ctx might actually be NULL, in which case we just ignore this.
    (void) ai;
    (void) gotfin;
    conn_tcp_freebsd_t* conn = ctx;
    if (conn == NULL) {
        // We got data on a socket on the accept queue that hasn't been accepted yet
        return;
    }
    mutex_lock(&conn->lock);
    assert(conn->hasactive && !conn->haspassive);
    conn->errstat = 0;
    cond_signal(&conn->sfields.active.receive_cond);
    mutex_unlock(&conn->lock);
}

static void conn_tcp_freebsd_connectionLost(uint8_t ai, uint8_t how, void* ctx)
{
    (void) ai;
    (void) how;
    conn_tcp_freebsd_t* conn = ctx;
    mutex_lock(&conn->lock);
    assert(conn->hasactive && !conn->haspassive);
    conn->errstat = -((int) how);
    cond_broadcast(&conn->sfields.active.connect_cond);
    cond_broadcast(&conn->sfields.active.receive_cond);
    cond_broadcast(&conn->sfields.active.send_cond);
    mutex_unlock(&conn->lock);
}

static acceptArgs_t conn_tcp_freebsd_acceptReady(uint8_t pi, void* ctx)
{
    /* To be returned after filling in members. */
    acceptArgs_t args;

    assert(ctx != NULL);
    conn_tcp_freebsd_t* conn = ctx;

    mutex_lock(&conn->lock);
    assert(conn->haspassive && !conn->hasactive);

    void* recvbuf = conn_tcp_freebsd_zalloc(RECV_BUF_LEN + REASS_BMP_LEN);
    if (recvbuf == NULL) {
        DEBUG("Out of memory in acceptReady\n");
        goto fail;
    }

    int asockid = bsdtcp_active_socket(conn_tcp_freebsd_connectDone,
        conn_tcp_freebsd_sendDone, conn_tcp_freebsd_receiveReady,
        conn_tcp_freebsd_connectionLost, NULL);

    args.asockid = asockid;
    args.recvbuf = recvbuf;
    args.recvbuflen = RECV_BUF_LEN;
    args.reassbmp = ((uint8_t*) args.recvbuf) + RECV_BUF_LEN;

done:
    mutex_unlock(&conn->lock);
    return args;

fail:
    args.asockid = -1;
    args.recvbuf = NULL;
    args.recvbuflen = 0;
    args.reassbmp = NULL;

    goto done;
}

static bool conn_tcp_freebsd_acceptDone(uint8_t pi, struct sockaddr_in6* faddr, int ai, void* ctx)
{
    (void) pi;
    (void) faddr;

    int putidx;

    conn_tcp_freebsd_t* conn = ctx;
    assert(conn != NULL);

    mutex_lock(&conn->lock);
    assert(conn->haspassive && !conn->hasactive);


    putidx = cib_put(&conn->sfields.passive.accept_cib);
    if (putidx == -1) {
        DEBUG("accept queue full\n");
        mutex_unlock(&conn->lock);
        return false;
    }
    conn->sfields.passive.accept_queue[putidx] = ai;

    cond_signal(&conn->sfields.passive.accept_cond);

    mutex_unlock(&conn->lock);

    return true;
}


static void conn_tcp_general_init(conn_tcp_freebsd_t* conn, uint16_t port)
{
    conn->l3_type = GNRC_NETTYPE_IPV6;
    conn->l4_type = GNRC_NETTYPE_TCP;

    conn->local_port = port;

    mutex_init(&conn->lock);
    conn->errstat = 0;
    conn->hasactive = false;
    conn->haspassive = false;
}

static void conn_tcp_passive_clear(conn_tcp_freebsd_t* conn)
{
    if (conn->haspassive) {
        int asockidx;

        conn->haspassive = false;
        bsdtcp_close(conn->sfields.passive.psock);
        cond_broadcast(&conn->sfields.passive.accept_cond);

        while ((asockidx = cib_get(&conn->sfields.passive.accept_cib)) != -1) {
            bsdtcp_close(conn->sfields.passive.accept_queue[asockidx]);
        }
        conn_tcp_freebsd_zfree(conn->sfields.passive.accept_queue);
    }
}

static void conn_tcp_active_clear(conn_tcp_freebsd_t* conn)
{
    if (conn->hasactive) {
        conn->hasactive = false;
        mutex_lock(&conn->sfields.active.connect_lock);
        conn_tcp_freebsd_zfree(conn->sfields.active.recvbuf);
        bsdtcp_close(conn->sfields.active.asock);
        cond_broadcast(&conn->sfields.active.connect_cond);
        cond_broadcast(&conn->sfields.active.receive_cond);
        cond_broadcast(&conn->sfields.active.send_cond);
        _free_sendstates(conn, (uint32_t) 0xFFFFFFFFu);
        assert(conn->sfields.active.in_send_buffer == 0);
    }
}

static bool conn_tcp_active_set(conn_tcp_freebsd_t* conn, int asock)
{
    conn_tcp_passive_clear(conn);
    if (!conn->hasactive) {
        conn->hasactive = true;
        if (asock == -1) {
            conn->sfields.active.asock = bsdtcp_active_socket(conn_tcp_freebsd_connectDone,
                conn_tcp_freebsd_sendDone, conn_tcp_freebsd_receiveReady,
                conn_tcp_freebsd_connectionLost, conn);
            if (conn->sfields.active.asock == -1) {
                conn->hasactive = false;
                return false;
            }

            conn->sfields.active.recvbuf = conn_tcp_freebsd_zalloc(RECV_BUF_LEN + REASS_BMP_LEN);
            if (conn->sfields.active.recvbuf == NULL) {
                conn->hasactive = false;
                bsdtcp_close(conn->sfields.active.asock);
                conn->sfields.active.asock = -1;
                return false;
            }

        } else {
            conn->sfields.active.asock = asock;
            /* How to get the recvbuf? */
        }
        bsdtcp_bind(conn->sfields.active.asock, conn->local_port);

        mutex_init(&conn->sfields.active.connect_lock);
        cond_init(&conn->sfields.active.connect_cond);
        cond_init(&conn->sfields.active.receive_cond);
        cond_init(&conn->sfields.active.send_cond);

        conn->sfields.active.send_head = NULL;
        conn->sfields.active.send_tail = NULL;
        conn->sfields.active.in_send_buffer = 0;
    }
    return true;
}

static bool conn_tcp_passive_set(conn_tcp_freebsd_t* conn, int queue_len)
{
    assert(queue_len >= 0 && queue_len < (1 << (8 * sizeof(int) - 2)));
    conn_tcp_active_clear(conn);
    if (!conn->haspassive) {
        conn->haspassive = true;
        conn->sfields.passive.psock = bsdtcp_passive_socket(conn_tcp_freebsd_acceptReady, conn_tcp_freebsd_acceptDone, conn);
        if (conn->sfields.passive.psock == -1) {
            conn->haspassive = false;
            return false;
        }
        bsdtcp_bind(conn->sfields.passive.psock, conn->local_port);

        cond_init(&conn->sfields.passive.accept_cond);

        /* Set adj_queue_len to the power of two above queue_len. */
        unsigned int adj_queue_len = 1;
        while (queue_len != 0) {
            queue_len >>= 1;
            adj_queue_len <<= 1;
        }
        adj_queue_len >>= 1;

        cib_init(&conn->sfields.passive.accept_cib, adj_queue_len);

        conn->sfields.passive.accept_queue = conn_tcp_freebsd_zalloc(adj_queue_len * sizeof(int));
        if (conn->sfields.passive.accept_queue == NULL && adj_queue_len != 0) {
            conn->haspassive = false;
            bsdtcp_close(conn->sfields.passive.psock);
            conn->sfields.passive.psock = -1;
            return false;
        }
    }
    return true;
}

int conn_tcp_create(conn_tcp_freebsd_t *conn, const void *addr, size_t addr_len, int family,
                    uint16_t port)
{
    conn->l4_type = GNRC_NETTYPE_TCP;
    switch (family) {
#ifdef MODULE_GNRC_IPV6
        case AF_INET6:
            if (addr_len != sizeof(ipv6_addr_t)) {
                return -EINVAL;
            }
            if (gnrc_conn6_set_local_addr((uint8_t*) &conn->local_addr, addr)) {
                conn_tcp_general_init(conn, port);
            }
            else {
                return -EADDRNOTAVAIL;
            }
            break;
#endif
        default:
            (void)addr;
            (void)addr_len;
            (void)port;
            return -EAFNOSUPPORT;
    }
    return 0;
}

void conn_tcp_close(conn_tcp_freebsd_t *conn)
{
    mutex_lock(&conn->lock);
    assert(!(conn->hasactive && conn->haspassive));
    conn_tcp_active_clear(conn);
    conn_tcp_passive_clear(conn);
    mutex_unlock(&conn->lock);
}

int conn_tcp_getlocaladdr(conn_tcp_freebsd_t *conn, void *addr, uint16_t *port)
{
    mutex_lock(&conn->lock);
    memcpy(addr, &conn->local_addr, sizeof(ipv6_addr_t));
    *port = conn->local_port;
    mutex_unlock(&conn->lock);
    return 0;
}

int conn_tcp_getpeeraddr(conn_tcp_freebsd_t *conn, void *addr, uint16_t *port)
{
    struct in6_addr* addrptr;
    uint16_t* portptr;
    mutex_lock(&conn->lock);
    if (!conn->hasactive || !bsdtcp_isestablished(conn->sfields.active.asock))
    {
        mutex_unlock(&conn->lock);
        return -ENOTCONN;
    }
    bsdtcp_peerinfo(conn->sfields.active.asock, &addrptr, &portptr);
    memcpy(addr, addrptr, sizeof(struct in6_addr));
    *port = *portptr;
    mutex_unlock(&conn->lock);
    return 0;
}

int conn_tcp_connect(conn_tcp_freebsd_t *conn, const void *addr, size_t addr_len, uint16_t port)
{
    int rv;

    struct sockaddr_in6 faddrport;

    mutex_lock(&conn->lock);
    if (addr_len != sizeof(struct in6_addr)) {
        rv = -EAFNOSUPPORT;
        goto unlockreturn;
    }
    memcpy(&faddrport.sin6_addr, addr, addr_len);
    faddrport.sin6_port = htons(port);
    bool res = conn_tcp_active_set(conn, -1);
    if (!res) {
        rv = -ENOMEM;
        goto unlockreturn;
    }

    mutex_lock(&conn->sfields.active.connect_lock);
    if (bsdtcp_isestablished(conn->sfields.active.asock)) {
        rv = -EISCONN;
        goto unlockboth;
    }
    conn->errstat = 0;
    int error = bsdtcp_connect(conn->sfields.active.asock, &faddrport,
        conn->sfields.active.recvbuf, RECV_BUF_LEN,
        ((uint8_t*) conn->sfields.active.recvbuf) + RECV_BUF_LEN);
    if (error != 0) {
        rv = -error;
        goto unlockboth;
    }

    /* Wait until either connection done OR connection lost */
    cond_wait(&conn->sfields.active.connect_cond, &conn->lock);

    rv = conn->errstat;

unlockboth:
    mutex_unlock(&conn->sfields.active.connect_lock);
unlockreturn:
    mutex_unlock(&conn->lock);
    return rv;
}

int conn_tcp_listen(conn_tcp_freebsd_t *conn, int queue_len)
{
    int rv;
    mutex_lock(&conn->lock);
    bool res = conn_tcp_passive_set(conn, queue_len);
    if (!res) {
        rv = -ENOMEM;
        goto unlockreturn;
    }

    rv = -bsdtcp_listen(conn->sfields.passive.psock);

unlockreturn:
    mutex_unlock(&conn->lock);
    return rv;
}

int conn_tcp_accept(conn_tcp_freebsd_t* conn, conn_tcp_freebsd_t* out_conn)
{
    mutex_lock(&conn->lock);
    if (!conn->haspassive) {
        mutex_unlock(&conn->lock);
        return -EINVAL;
    }

    assert(!conn->hasactive);

    int asockidx;
    while ((asockidx = cib_get(&conn->sfields.passive.accept_cib)) == -1) {
        cond_wait(&conn->sfields.passive.accept_cond, &conn->lock);
    }

    int asock = conn->sfields.passive.accept_queue[asockidx];

    memcpy(&out_conn->local_addr, &conn->local_addr, sizeof(ipv6_addr_t));
    conn_tcp_general_init(out_conn, conn->local_port);

    mutex_lock(&out_conn->lock);
    conn_tcp_active_set(out_conn, asock);
    int rv = bsdtcp_set_ctx(asock, out_conn);
    assert(rv == 0);
    mutex_unlock(&out_conn->lock);

    mutex_unlock(&conn->lock);
    return 0;
}

int conn_tcp_recv(conn_tcp_freebsd_t *conn, void *data, size_t max_len)
{
    size_t bytes_read;
    int error;

    assert(conn->hasactive && !conn->haspassive);
    mutex_lock(&conn->lock);

    conn->errstat = 0;
    error = bsdtcp_receive(conn->sfields.active.asock, data, max_len, &bytes_read);
    while (bytes_read == 0 && error == 0 && conn->errstat == 0) {
        cond_wait(&conn->sfields.active.receive_cond, &conn->lock);
        error = bsdtcp_receive(conn->sfields.active.asock, data, max_len, &bytes_read);
    }

    mutex_unlock(&conn->lock);

    if (error != 0) {
        return -error;
    } else if (conn->errstat != 0) {
        return conn->errstat;
    }
    return (int) bytes_read;
}

/* SEND POLICY
 * If a buffer is smaller than or equal to SENDMAXCOPY bytes, then
 * COPYBUFSIZE bytes are allocated, and the buffer is copied into the
 * space. This allows the TCP stack to coalesce small buffers within the
 * remaining space in COPYBUFSIZE.
 * Otherwise, the TCP stack is provided a reference to the buffer, with no
 * extra space.
 */
int conn_tcp_send(conn_tcp_freebsd_t *conn, const void* data, size_t len)
{
    int error = 0;
    struct lbufent* bufent;
    struct conn_tcp_freebsd_send_state* sstate;

    mutex_lock(&conn->lock);
    assert(conn->hasactive && !conn->haspassive);

    while (len > 0 && error == 0) {
        /*
         * Look at the remaining space in the send buffer to figure out how much
         * we can send.
         */
        while (conn->sfields.active.in_send_buffer >= SENDBUFSIZE) {
            assert(conn->sfields.active.in_send_buffer == SENDBUFSIZE);
            cond_wait(&conn->sfields.active.send_cond, &conn->lock);
        }
        const char* buffer = data;
        size_t buflen = SENDBUFSIZE - conn->sfields.active.in_send_buffer;
        if (len < buflen) {
            buflen = len;
        }

        bool copy = (buflen <= SENDMAXCOPY);
        if (copy) {
            if (extracopybuf == NULL) {
                sstate = conn_tcp_freebsd_zalloc(sizeof(*sstate) + COPYBUFSIZE);
                sstate->buflen = COPYBUFSIZE;
            } else {
                sstate = extracopybuf;
                assert(sstate->buflen == COPYBUFSIZE);
                extracopybuf = NULL;
            }
        } else {
            sstate = conn_tcp_freebsd_zalloc(sizeof(*sstate) + buflen);
            sstate->buflen = buflen;
        }

        if (sstate == NULL) {
            error = ENOMEM;
            goto unlockreturn;
        }
        sstate->next = NULL;

        bufent = &sstate->entry;
        bufent->iov.iov_next = NULL;
        bufent->iov.iov_len = buflen;

        bufent->iov.iov_base = (uint8_t*) (sstate + 1);
        bufent->extraspace = (copy ? (COPYBUFSIZE - buflen) : buflen);
        memcpy(bufent->iov.iov_base, buffer, buflen);

        int state;

        error = (int) bsdtcp_send(conn->sfields.active.asock, bufent, &state);

        if (state == 1) {
            /* The TCP stack has a reference to this buffer, and we must keep track of it. */
            if (conn->sfields.active.send_tail == NULL) {
                conn->sfields.active.send_head = sstate;
            } else {
                conn->sfields.active.send_tail->next = sstate;
            }
            conn->sfields.active.send_tail = sstate;
        } else {
            /* Either the send failed, or this was copied into the last buffer already. */
            if (copy) {
                assert(extracopybuf == NULL);
                /* Cache this copy, to avoid another dynamic memory allocation */
                extracopybuf = sstate;
            } else {
                conn_tcp_freebsd_zfree(sstate);
            }
        }

        if (state != 0) {
            /* The send didn't fail, so we need to keep track of queued bytes. */
            conn->sfields.active.in_send_buffer += buflen;
        }

        buffer += buflen;
        len -= buflen;
    }

unlockreturn:
    mutex_unlock(&conn->lock);
    return -error;
}
