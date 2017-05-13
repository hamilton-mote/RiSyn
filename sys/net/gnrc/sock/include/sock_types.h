/*
 * Copyright (C) 2016 Martine Lenders <mlenders@inf.fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_gnrc_sock   GNRC-specific implementation of the sock API
 * @ingroup     net_gnrc
 * @brief       Provides an implementation of the @ref net_sock by the
 *              @ref net_gnrc
 *
 * @{
 *
 * @file
 * @brief   GNRC-specific types and function definitions
 *
 * @author  Martine Lenders <mlenders@inf.fu-berlin.de>
 */
#ifndef SOCK_TYPES_H
#define SOCK_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "mbox.h"
#include "net/af.h"
#include "net/gnrc.h"
#include "net/gnrc/netreg.h"
#include "net/sock/ip.h"
#include "net/sock/udp.h"

/* These two are needed for FreeBSD TCP. */
#include "condition.h"
#include "net/tcp_freebsd.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SOCK_MBOX_SIZE
#define SOCK_MBOX_SIZE      (8)         /**< Size for gnrc_sock_reg_t::mbox_queue */
#endif

/**
 * @brief   sock @ref net_gnrc_netreg info
 * @internal
 */
typedef struct gnrc_sock_reg {
#ifdef MODULE_GNRC_SOCK_CHECK_REUSE
    struct gnrc_sock_reg *next;         /**< list-like for internal storage */
#endif
    gnrc_netreg_entry_t entry;          /**< @ref net_gnrc_netreg entry for mbox */
    mbox_t mbox;                        /**< @ref core_mbox target for the sock */
    msg_t mbox_queue[SOCK_MBOX_SIZE];   /**< queue for gnrc_sock_reg_t::mbox */
} gnrc_sock_reg_t;

/**
 * @brief   Raw IP sock type
 * @internal
 */
struct sock_ip {
    gnrc_sock_reg_t reg;                /**< netreg info */
    sock_ip_ep_t local;                 /**< local end-point */
    sock_ip_ep_t remote;                /**< remote end-point */
    uint16_t flags;                     /**< option flags */
};

/**
 * @brief   UDP sock type
 * @internal
 */
struct sock_udp {
    gnrc_sock_reg_t reg;                /**< netreg info */
    sock_udp_ep_t local;                /**< local end-point */
    sock_udp_ep_t remote;               /**< remote end-point */
    uint16_t flags;                     /**< option flags */
};

/*
 * @brief    Used in TCP FREEBSD sock type
 * @internal
 */
struct sock_tcp_freebsd_send_state {
    size_t buflen;
    struct sock_tcp_freebsd_send_state* next;
    struct lbufent entry;
};

/*
 * @brief    Used in TCP FREEBSD sock type
 * @internal
 */
struct sock_tcp_freebsd_accept_queue_entry {
    int asockid;
    void* recvbuf;
};

/*
 * @brief    TCP FREEBSD sock type
 * @internal
 */
struct sock_tcp_freebsd {
    gnrc_nettype_t l3_type;
    gnrc_nettype_t l4_type;
    gnrc_netreg_entry_t netreg_entry; // to follow the inheritance

    ipv6_addr_t local_addr;
    uint16_t local_port;

    mutex_t lock;
    union {
        struct {
            int asock;
            void* recvbuf;
            mutex_t connect_lock;
            condition_t connect_cond;
            condition_t receive_cond;
            condition_t send_cond;

            struct sock_tcp_freebsd_send_state* send_head;
            struct sock_tcp_freebsd_send_state* send_tail;
            size_t in_send_buffer;
        } active;
        struct {
            int psock;
            condition_t accept_cond;

            /* Circular buffer for accept queue. */
            cib_t accept_cib;
            struct sock_tcp_freebsd_accept_queue_entry* accept_queue;
        } passive;
    } sfields; /* specific fields */
    int errstat;
    bool hasactive;
    bool haspassive;
};

#ifdef __cplusplus
}
#endif

#endif /* SOCK_TYPES_H */
/** @} */
