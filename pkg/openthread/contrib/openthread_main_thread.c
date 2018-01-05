/*
 * Copyright (C) 2017 Fundacion Inria Chile
 * Copyright (C) 2017 UC Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net
 * @file
 * @brief       Implementation of OpenThread main functions
 *
 * @author      Jose Ignacio Alamos <jialamos@uc.cl>
 * @author      Hyung-Sin Kim <hs.kim@cs.berkeley.edu>
 * @}
 */

#include <assert.h>

#include "openthread/platform/uart.h"
#include "openthread/cli.h"
#include "openthread/ip6.h"
#include "openthread/instance.h"
#include "openthread/thread.h"
#include "openthread/platform/alarm-milli.h"
#include "openthread/tasklet.h"
#include "ot.h"
#include "random.h"
#include "thread.h"
#include "xtimer.h"

#ifdef MODULE_OPENTHREAD_NCP_FTD
#include "openthread/ncp.h"
#endif

#ifdef MODULE_AT86RF2XX
#include "at86rf2xx.h"
#include "at86rf2xx_params.h"
#endif

#define ENABLE_DEBUG (0)
#include "debug.h"

#ifdef MODULE_AT86RF2XX     /* is mutual exclusive with above ifdef */
#define OPENTHREAD_NETIF_NUMOF        (sizeof(at86rf2xx_params) / sizeof(at86rf2xx_params[0]))
#endif

static otInstance *sInstance;

static xtimer_t ot_timer;

#ifdef MODULE_AT86RF2XX
static at86rf2xx_t at86rf2xx_dev;
#endif

#define OPENTHREAD_QUEUE_LEN      (8)
static msg_t _queue[OPENTHREAD_QUEUE_LEN];
static char ot_main_thread_stack[THREAD_STACKSIZE_MAIN+1004];
static kernel_pid_t main_pid;

static uint8_t rx_buf[OPENTHREAD_NETDEV_BUFLEN];
static uint8_t tx_buf[OPENTHREAD_NETDEV_BUFLEN];

static char ot_event_thread_stack[THREAD_STACKSIZE_MAIN+4];
static kernel_pid_t event_pid;


/* get OpenThread instance */
otInstance* openthread_get_instance(void) {
    return sInstance;
}

/* get OpenThread thread main pid */
kernel_pid_t openthread_get_main_pid(void) {
    return main_pid;
}

/* get OpenThread thread event pid */
kernel_pid_t openthread_get_event_pid(void) {
    return event_pid;
}

netdev_t* openthread_get_netdev(void) {
    return (netdev_t*) &at86rf2xx_dev;
}

xtimer_t* openthread_get_timer(void) {
    return &ot_timer;
}

bool openthread_main_stack_overflow_check(void) {
    if (ot_main_thread_stack[0] == 0xA9 && ot_main_thread_stack[1] == 0x3C &&
        ot_main_thread_stack[2] == 0x08 && ot_main_thread_stack[3] == 0x29) {
        return false;
    } 
    return true;
} 

bool openthread_event_stack_overflow_check(void) {
    if (ot_event_thread_stack[0] == 0x82 && ot_event_thread_stack[1] == 0xAE &&
        ot_event_thread_stack[2] == 0xCA && ot_event_thread_stack[3] == 0x11) {
        return false;
    } 
    return true;
} 

uint8_t ot_call_command(char* command, void *arg, void* answer) {
    ot_job_t job;

    job.command = command;
    job.arg = arg;
    job.answer = answer;

    msg_t msg, reply;
    msg.type = OPENTHREAD_JOB_MSG_TYPE_EVENT;
    msg.content.ptr = &job;
    msg_send_receive(&msg, &reply, openthread_get_main_pid());
    return (uint8_t)reply.content.value;
}

/* OpenThread will call this when switching state from empty tasklet to non-empty tasklet. */
void otTaskletsSignalPending(otInstance *aInstance) {
    (void) aInstance;
}

/* Interupt handler for OpenThread timer event */
void _timer_cb(void* arg) {
    msg_t msg;
	msg.type = OPENTHREAD_XTIMER_MSG_TYPE_EVENT;
	msg_send(&msg, event_pid);
}

/* Interupt handler for OpenThread netdev thread */
static void _event_cb(netdev_t *dev, netdev_event_t event) {
    switch (event) {
        case NETDEV_EVENT_ISR:
            {
                msg_t msg;
                msg.type = OPENTHREAD_NETDEV_MSG_TYPE_EVENT;
                msg.content.ptr = dev;

                if (msg_send(&msg,main_pid) <= 0) {
                    DEBUG("ot_main: possibly lost interrupt.\n");
                }
                break;
            }
        case NETDEV_EVENT_ISR2:
            {
                msg_t msg;
                msg.type = OPENTHREAD_NETDEV_MSG_TYPE_EVENT;
                msg.content.ptr = dev;

                if (msg_send(&msg, openthread_get_event_pid()) <= 0) {
                    DEBUG("ot_event: possibly lost interrupt.\n");
                }
                break;
            }
        case NETDEV_EVENT_RX_COMPLETE:
            recv_pkt(sInstance, dev);
            break;
        case NETDEV_EVENT_TX_COMPLETE:
        case NETDEV_EVENT_TX_COMPLETE_DATA_PENDING:
        case NETDEV_EVENT_TX_NOACK:
        case NETDEV_EVENT_TX_MEDIUM_BUSY:
            sent_pkt(sInstance, event);
            break;
        default:
            break;
    }
}

static void *_openthread_main_thread(void *arg) {
    main_pid = thread_getpid();

    msg_init_queue(_queue, OPENTHREAD_QUEUE_LEN);
    msg_t msg, reply;

    netdev_t* netdev = (netdev_t *) &at86rf2xx_dev;

    ot_job_t *job;
    serial_msg_t* serialBuffer;

    /* enable OpenThread UART */
    otPlatUartEnable();
    DEBUG("ot_main: UART setting is OK\n");

    /* init OpenThread */
    sInstance = otInstanceInitSingle();
    DEBUG("ot_main: OT-instance setting is OK\n");
    
#if defined(MODULE_OPENTHREAD_CLI_FTD) || defined(MODULE_OPENTHREAD_CLI_MTD)
    otCliUartInit(sInstance);
    DEBUG("ot-main: OT-UART initialization is OK\n");
    /* Init default parameters */
    otPanId panid = OPENTHREAD_PANID;
    uint8_t channel = OPENTHREAD_CHANNEL;
    otLinkSetPanId(sInstance, panid);
    otLinkSetChannel(sInstance, channel);
    /* Bring up the IPv6 interface  */
    otIp6SetEnabled(sInstance, true);
    DEBUG("ot_main: OT-IPv6 setting is OK\n");
    /* Start Thread protocol operation */
    otThreadSetEnabled(sInstance, true);
    DEBUG("ot_main: OT-initialization is OK\n");
#endif

#ifdef MODULE_OPENTHREAD_NCP_FTD
    otNcpInit(sInstance);
    DEBUG("ot_main: OT-NCP setting is OK\n");
#endif

#if OPENTHREAD_ENABLE_DIAG
    diagInit(sInstance);
#endif

    DEBUG("ot_main: START!\n");

    while (1) {
        otTaskletsProcess(sInstance);
        if (otTaskletsArePending(sInstance) == false) {
            //DEBUG("****** ot_main sleep ******\n");
            if (openthread_main_stack_overflow_check()) {
                DEBUG("\n\n\n\n\n\n\n\n\n\n\n\nstack overflow\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
                NVIC_SystemReset();
            }
            msg_receive(&msg);
            DEBUG("\n****** ot_main wakeup\n");
            switch (msg.type) {
                case OPENTHREAD_NETDEV_MSG_TYPE_EVENT:
                    /* Received an event from driver */
                    //DEBUG("ot_main: OPENTHREAD_NETDEV_MSG_TYPE_EVENT received\n");
                    netdev->driver->isr(netdev);
                    break;
                case OPENTHREAD_XTIMER_MSG_TYPE_EVENT:
                    /* Tell OpenThread a time event was received */
                    DEBUG("ot_main: timer fired\n");//OPENTHREAD_XTIMER_MSG_TYPE_EVENT received\n");
                    otPlatAlarmMilliFired(sInstance);
                    break;
                case OPENTHREAD_SERIAL_MSG_TYPE_EVENT:
                    /* Tell OpenThread about the reception of a CLI command */
                    DEBUG("ot_main: serial\n");//OPENTHREAD_SERIAL_MSG_TYPE_SEND received\n");
                    serialBuffer = (serial_msg_t*)msg.content.ptr;
                    DEBUG("%s", serialBuffer->buf);
                    otPlatUartReceived((uint8_t*) serialBuffer->buf,serialBuffer->length);
                    serialBuffer->serial_buffer_status = OPENTHREAD_SERIAL_BUFFER_STATUS_FREE;
                    break;
                case OPENTHREAD_JOB_MSG_TYPE_EVENT:
                    DEBUG("ot_main: OPENTHREAD_JOB_MSG_TYPE_EVENT received\n");
                    job = msg.content.ptr;
                    reply.content.value = ot_exec_command(sInstance, job->command, job->arg, job->answer);
                    msg_reply(&msg, &reply);
                    break;
            }
        }
    }

    return NULL;
}

void openthread_bootstrap(void)
{
    /* init random */
    ot_random_init();

    /* set openthread timer callback */
    ot_timer.callback = _timer_cb;

    /* setup netdev modules */
#ifdef MODULE_AT86RF2XX
    at86rf2xx_setup(&at86rf2xx_dev, &at86rf2xx_params[0]);
    netdev_t *netdev = (netdev_t *) &at86rf2xx_dev;
#endif

    netdev->driver->init(netdev);
    netdev->event_callback = _event_cb;
    netopt_enable_t enable = NETOPT_ENABLE;
    netdev->driver->set(netdev, NETOPT_RX_END_IRQ, &enable, sizeof(enable));
    netdev->driver->set(netdev, NETOPT_TX_END_IRQ, &enable, sizeof(enable));
    openthread_radio_init(netdev, tx_buf, rx_buf);
    DEBUG("RADIO setting is OK\n");

    ot_main_thread_stack[0] = 0xA9;
    ot_main_thread_stack[1] = 0x3C;
    ot_main_thread_stack[2] = 0x08;
    ot_main_thread_stack[3] = 0x29;
    
    ot_event_thread_stack[0] = 0x82;
    ot_event_thread_stack[1] = 0xAE;
    ot_event_thread_stack[2] = 0xCA; 
    ot_event_thread_stack[3] = 0x11;

    /* init two threads for openthread */
    event_pid = openthread_event_init(ot_event_thread_stack, sizeof(ot_event_thread_stack),
                         THREAD_PRIORITY_MAIN + 1, "openthread_event"); 
    main_pid = thread_create(ot_main_thread_stack, sizeof(ot_main_thread_stack),
                         THREAD_PRIORITY_MAIN + 2, THREAD_CREATE_STACKTEST,
                         _openthread_main_thread, NULL, "openthread_main");   
}
