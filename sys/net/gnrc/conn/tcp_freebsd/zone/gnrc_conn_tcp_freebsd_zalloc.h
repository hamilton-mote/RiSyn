/*
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef GNRC_CONN_TCP_FREEBSD_ZALLOC_H_
#define GNRC_CONN_TCP_FREEBSD_ZALLOC_H_

#include "memmgr.h"

#define ENABLE_DEBUG (0)

#include "debug.h"

static bool initialized = false;

static inline void conn_tcp_freebsd_zone_init(void)
{
    if (!initialized) {
        initialized = true;
        memmgr_init();
    }
}

static inline void* conn_tcp_freebsd_zalloc(unsigned long numbytes) {
    if (numbytes == 0) {
        return NULL;
    }
    conn_tcp_freebsd_zone_init();
    void* p = memmgr_alloc(numbytes);
    DEBUG("Allocating %lu bytes: %p\n", numbytes, p);
    return p;
}

static inline void conn_tcp_freebsd_zfree(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    assert(initialized);
    DEBUG("Freeing %p\n", ptr);
    memmgr_free(ptr);
}

#endif
