/*
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef GNRC_SOCK_TCP_FREEBSD_ZALLOC_H_
#define GNRC_SOCK_TCP_FREEBSD_ZALLOC_H_

#include "memmgr.h"
#include "mutex.h"

#include "debug.h"

static bool initialized = false;

mutex_t sock_tcp_freebsd_zalloc_mutex = MUTEX_INIT;

static inline void sock_tcp_freebsd_zone_init(void)
{
    if (!initialized) {
        initialized = true;
        memmgr_init();
    }
}

static inline void* sock_tcp_freebsd_zalloc(unsigned long numbytes) {
    if (numbytes == 0) {
        return NULL;
    }
    mutex_lock(&sock_tcp_freebsd_zalloc_mutex);
    sock_tcp_freebsd_zone_init();
    void* p = memmgr_alloc(numbytes);
    mutex_unlock(&sock_tcp_freebsd_zalloc_mutex);
    DEBUG("Allocating %lu bytes: %p\n", numbytes, p);
    return p;
}

static inline void sock_tcp_freebsd_zfree(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    mutex_lock(&sock_tcp_freebsd_zalloc_mutex);
    assert(initialized);
    memmgr_free(ptr);
    mutex_unlock(&sock_tcp_freebsd_zalloc_mutex);
    DEBUG("Freeing %p\n", ptr);
}

#endif
