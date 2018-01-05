/*
 * Copyright (C) 2017 Fundacion Inria Chile
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    pkg_openthread_cli   OpenThread
 * @ingroup     pkg_openthread
 * @brief       An open source implementation of Thread stack
 * @see         https://github.com/openthread/openthread
 *
 * Thread if a mesh oriented network stack running for IEEE802.15.4 networks.
 * @{
 *
 * @file
 *
 * @author      Jose Ignacio Alamos <jialamos@uc.cl>
 * @author      Baptiste Clenet <bapclenet@gmail.com>
 */

#ifndef OT_H
#define OT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "net/netopt.h"
#include "net/ieee802154.h"
#include "net/ethernet.h"
#include "net/gnrc/netdev.h"
#include "thread.h"
#include "openthread/types.h"
#include "openthread/platform/radio.h"

/**< xtimer message receiver event */
#define OPENTHREAD_XTIMER_MSG_TYPE_EVENT                    (0x2235)
/**< event indicating a serial (UART) message was sent to OpenThread */
#define OPENTHREAD_SERIAL_MSG_TYPE_EVENT                    (0x2236)
/**< event for frame reception */
#define OPENTHREAD_NETDEV_MSG_TYPE_EVENT                    (0x2239)
/**< event indicating an OT_JOB message */
#define OPENTHREAD_JOB_MSG_TYPE_EVENT                       (0x2241)
/**< number of serial reception buffer */
#define OPENTHREAD_NUMBER_OF_SERIAL_BUFFER                  (1U)
/**< sizeof in bytes the two first members of she serial structure */
#define OPENTHREAD_SIZEOF_LENGTH_AND_FREEBUFF               (4U)
#ifdef MODULE_OPENTHREAD_NCP_FTD
/**< sizeof the serial buffer */
#define OPENTHREAD_SERIAL_BUFFER_SIZE                       OPENTHREAD_SIZEOF_LENGTH_AND_FREEBUFF + 200
#else
/**< sizeof the serial buffer */
#define OPENTHREAD_SERIAL_BUFFER_SIZE                       OPENTHREAD_SIZEOF_LENGTH_AND_FREEBUFF + 100
#endif
/**< sizeof the spinel payload data */
#define OPENTHREAD_SERIAL_BUFFER__PAYLOAD_SIZE              OPENTHREAD_SERIAL_BUFFER_SIZE - OPENTHREAD_SIZEOF_LENGTH_AND_FREEBUFF
/**< error when no more buffer available */
#define OPENTHREAD_ERROR_NO_EMPTY_SERIAL_BUFFER             -1
/**< serial buffer ready to use */
#define OPENTHREAD_SERIAL_BUFFER_STATUS_FREE                (0x0001)
/**< serial buffer ready for processsing */
#define OPENTHREAD_SERIAL_BUFFER_STATUS_READY_TO_PROCESS    (0x0002)
/**< serial buffer payload full */
#define OPENTHREAD_SERIAL_BUFFER_STATUS_FULL                (0x0004)
/**< Max length for IEEE802154 frame */
#define IEEE802154_MAX_LENGTH                               (127U)
/**< Max length for a netdev buffer  */
#define OPENTHREAD_NETDEV_BUFLEN                            (IEEE802154_MAX_LENGTH)


/**
 * @brief   Struct containing a serial message
 */
typedef struct {
    uint16_t length;                                        /**< length of the message */
    uint16_t serial_buffer_status;                            /**< status of the buffer */
    uint8_t buf[OPENTHREAD_SERIAL_BUFFER__PAYLOAD_SIZE];    /**< buffer containing the message */
} serial_msg_t;

/**
 * @brief   Struct containing an OpenThread job
 */
typedef struct {
    const char *command;                    /**< A pointer to the job name string. */
    void *arg;                              /**< arg for the job **/
    void *answer;                           /**< answer from the job **/
} ot_job_t;

/**
 * @brief Gets packet from driver and tells OpenThread about the reception.
 *
 * @param[in]  aInstance          pointer to an OpenThread instance
 */
void recv_pkt(otInstance *aInstance, netdev_t *dev);

/**
 * @brief   Inform OpenThread when tx is finished
 *
 * @param[in]  aInstance          pointer to an OpenThread instance
 * @param[in]  dev                pointer to a netdev interface
 * @param[in]  event              just occurred netdev event
 */
void sent_pkt(otInstance *aInstance, netdev_event_t event);

/**
 * @brief   Bootstrap OpenThread
 */
void openthread_bootstrap(void);

/**
 * @brief   Init OpenThread radio
 *
 * @param[in]  dev                pointer to a netdev interface
 * @param[in]  tb                 pointer to the TX buffer designed for OpenThread
 * @param[in]  event              pointer to the RX buffer designed for Open_Thread
 */
void openthread_radio_init(netdev_t *dev, uint8_t *tb, uint8_t *rb);

/**
 * @brief   Starts OpenThread thread.
 *
 * @param[in]  stack              pointer to the stack designed for OpenThread
 * @param[in]  stacksize          size of the stack
 * @param[in]  priority           priority of the OpenThread stack
 * @param[in]  name               name of the OpenThread stack
 * @param[in]  netdev             pointer to the netdev interface
 *
 * @return  PID of OpenThread thread
 * @return  -EINVAL if there was an error creating the thread
 */
//int openthread_rx_init(char *stack, int stacksize, char priority, const char *name, netdev_t *netdev);

/**
 * @brief   Starts OpenThread thread.
 *
 * @param[in]  stack              pointer to the stack designed for OpenThread timer
 * @param[in]  stacksize          size of the stack
 * @param[in]  priority           priority of the OpenThread timer stack
 * @param[in]  name               name of the OpenThread timer stack
 *
 * @return  PID of OpenThread event thread
 * @return  -EINVAL if there was an error creating the thread
 */
int openthread_event_init(char *stack, int stacksize, char priority, const char *name);

/**
 * @brief   get PID of OpenThread main thread.
 *
 * @return  PID of OpenThread main thread
 */
kernel_pid_t openthread_get_main_pid(void);

/**
 * @brief   get PID of OpenThread Critical Event thread.
 *
 * @return  PID of OpenThread Critical Event thread
 */
kernel_pid_t openthread_get_event_pid(void);

/**
 * @brief   get instance of OpenThread.
 *
 * @return  instance of OpenThread thread
 */
otInstance* openthread_get_instance(void);

/**
 * @brief   get timer of OpenThread.
 *
 * @return  timer of OpenThread thread
 */
xtimer_t* openthread_get_timer(void);

bool openthread_main_stack_overflow_check(void);
bool openthread_event_stack_overflow_check(void);

/**
 * @brief   Init OpenThread random
 */
void ot_random_init(void);

/**
 * @brief   Execute OpenThread command. Call this function only in OpenThread thread
 *
 * @param[in]   ot_instance     OpenThread instance
 * @param[in]   command         OpenThread command name
 * @param[in]   arg             arg for the command
 * @param[out]  answer          answer for the command
 *
 * @return  0 on success, 1 on error
 */
uint8_t ot_exec_command(otInstance *ot_instance, const char* command, void *arg, void* answer);

/**
 * @brief   Call OpenThread command in same thread as OT core (due to concurrency).
 *
 * @note    An OpenThread command allows direct calls to OpenThread API (otXXX functions) without worrying about concurrency
 * issues. All API calls should be made in OT_JOB type functions.
 *
 * @param[in]   command         name of the command to call
 * @param[in]   arg             arg for the command
 * @param[out]  answer          answer for the command
 *
 * @return  0 on success, 1 on error
 */
uint8_t ot_call_command(char* command, void *arg, void* answer);

#ifdef __cplusplus
}
#endif

#endif /* OT_H */
/** @} */
