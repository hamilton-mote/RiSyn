/*
 * Copyright (C) 2017 UC Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     BETS
 * @file
 * @brief       Implementation of OpenThread critical event thread
 *
 * @author      Hyung-Sin Kim <hs.kim@cs.berkeley.edu>
 * @}
 */

#include "thread.h"
#include "ot.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

#define OPENTHREAD_EVENT_QUEUE_LEN (3)
static msg_t _queue[OPENTHREAD_EVENT_QUEUE_LEN];
static kernel_pid_t _pid;

/* OpenThread critical event Thread */
static void *_openthread_event_thread(void *arg) {
    _pid = thread_getpid();

    msg_init_queue(_queue, OPENTHREAD_EVENT_QUEUE_LEN);
    msg_t msg;

    while (1) {
        msg_receive(&msg);            
        switch (msg.type) {
            case OPENTHREAD_XTIMER_MSG_TYPE_EVENT:
                /* Tell OpenThread a time event was received */
                DEBUG("\not_event: OPENTHREAD_XTIMER_MSG_TYPE_EVENT received\n");
                msg.type = OPENTHREAD_XTIMER_MSG_TYPE_EVENT;
                msg_send(&msg, openthread_get_main_pid());
                break;
            case OPENTHREAD_NETDEV_MSG_TYPE_EVENT:
                /* Received an event from driver */
                DEBUG("\not_event: OPENTHREAD_NETDEV_MSG_TYPE_EVENT received\n");
                msg.type = OPENTHREAD_NETDEV_MSG_TYPE_EVENT;
                msg_send(&msg, openthread_get_main_pid());
                break;
        }
        if (openthread_event_stack_overflow_check()) {
            DEBUG("\n\n\n\n\n\n\n\n\n\n\n\nevent thread stack overflow\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
            NVIC_SystemReset();
        }    
    }

    return NULL;
}

/* starts OpenThread critical event thread */
int openthread_event_init(char *stack, int stacksize, char priority, const char *name) {

    _pid = thread_create(stack, stacksize, priority, THREAD_CREATE_STACKTEST,
                         _openthread_event_thread, NULL, name);

    if (_pid <= 0) {
        return -EINVAL;
    }

    return _pid;
}
