/*
 * Copyright (C) 2016 UC Berkeley
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net
 * @file
 * @brief       Glue for netdev devices to netapi (listen-after-send protocol for leaf nodes)
 *				Duty-cycling protocol of Thread network
 *
 * @author      Hyung-Sin Kim <hs.kim@berkeley.edu>
 * @}
 */

#include <errno.h>



#include "msg.h"
#include "thread.h"

#include "net/gnrc.h"
#include "net/gnrc/nettype.h"
#include "net/netdev.h"

#include "net/gnrc/netdev.h"
#include "net/gnrc/lasmac/lasmac.h"
#include "net/ethernet/hdr.h"
#include "random.h"
#include "net/ieee802154.h"
#include <periph/gpio.h>


#include "xtimer.h"

#if LEAF_NODE

#define ENABLE_DEBUG    (0)
#include "debug.h"

#if defined(MODULE_OD) && ENABLE_DEBUG
#include "od.h"
#endif

#define NETDEV_NETAPI_MSG_QUEUE_SIZE 8
#define NETDEV_PKT_QUEUE_SIZE 4

static void _pass_on_packet(gnrc_pktsnip_t *pkt);

/*  Dutycycle state (INIT, SLEEP, TXDATAREQ, TXDATA, and LISTEN) */
typedef enum {
	DUTY_INIT,
	DUTY_SLEEP,
	DUTY_TX_DATAREQ,
	DUTY_TX_DATA,
	DUTY_TX_DATA_BEFORE_DATAREQ,
	DUTY_LISTEN,
} dutycycle_state_t;
dutycycle_state_t dutycycle_state = DUTY_INIT;

/* timer for wake-up scheduling */
xtimer_t timer;
uint8_t pending_num = 0;

/* A packet can be sent only when radio_busy = 0 */
bool radio_busy = false;

/* This is the packet being sent by the radio now */
gnrc_pktsnip_t *current_pkt;

bool additional_wakeup = false;
bool wakeup_while_sending = false;

// FIFO QUEUE
int msg_queue_add(msg_t* msg_queue, msg_t* msg) {
	if (pending_num < NETDEV_PKT_QUEUE_SIZE) {
		/* Add a packet to the last entry of the queue */
		msg_queue[pending_num].sender_pid = msg->sender_pid;
		msg_queue[pending_num].type = msg->type;
		msg_queue[pending_num].content.ptr = msg->content.ptr;
		DEBUG("\nqueue add success [%u/%u/%4x]\n", pending_num, msg_queue[pending_num].sender_pid,
				msg_queue[pending_num].type);
		pending_num++; /* Number of packets in the queue */
		return 1;
	} else {
		DEBUG("Queue loss at netdev\n");
		return 0;
	}
}

void msg_queue_remove_head(msg_t* msg_queue) {
	/* Remove queue head */
	DEBUG("remove queue [%u]\n", pending_num-1);
	gnrc_pktbuf_release(msg_queue[0].content.ptr);
	pending_num--;
	if (pending_num < 0) {
		DEBUG("NETDEV: Pending number error\n");
	}

    /* Update queue when more pending packets exist */
	if (pending_num) {
		for (int i=0; i<pending_num; i++) {
			msg_queue[i].sender_pid = msg_queue[i+1].sender_pid;
			msg_queue[i].type = msg_queue[i+1].type;
			msg_queue[i].content.ptr = msg_queue[i+1].content.ptr;
			if (msg_queue[i].sender_pid == 0 && msg_queue[i].type == 0) {
				break;
			}
		}
	}
	return;
}

void msg_queue_send(msg_t* msg_queue, gnrc_netdev_t* gnrc_lasmac_netdev) {
	gnrc_pktsnip_t *pkt = msg_queue[0].content.ptr;
	gnrc_pktsnip_t* temp_pkt = pkt;
	gnrc_pktsnip_t *p1, *p2;
	current_pkt = gnrc_pktbuf_add(NULL, temp_pkt->data, temp_pkt->size, temp_pkt->type);
	p1 = current_pkt;
	temp_pkt = temp_pkt->next;
	while(temp_pkt) {
		p2 = gnrc_pktbuf_add(NULL, temp_pkt->data, temp_pkt->size, temp_pkt->type);
		p1->next = p2;
		p1 = p1->next;
		temp_pkt = temp_pkt->next;
	}
	radio_busy = true; /* radio is now busy */
	gnrc_lasmac_netdev->send(gnrc_lasmac_netdev, current_pkt);
}


/**
 * @brief   Function called by the dutycycle timer
 *
 * @param[in] event     type of event
 */
void dutycycle_cb(void* arg) {
	if (dutycycle_state == DUTY_TX_DATA) {
		dutycycle_state = DUTY_TX_DATA_BEFORE_DATAREQ;
		return; 
	}
	gnrc_netdev_t* gnrc_lasmac_netdev = (gnrc_netdev_t*) arg;
    msg_t msg;
	msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;
	msg_send(&msg, gnrc_lasmac_netdev->pid);
}

/**
 * @brief   Function called by the device driver on device events
 *
 * @param[in] event     type of event
 */
static void _event_cb(netdev_t *dev, netdev_event_t event)
{
	gnrc_netdev_t* gnrc_lasmac_netdev = (gnrc_netdev_t*)dev->context;
	if (event == NETDEV_EVENT_ISR) {
		msg_t msg;
		msg.type = NETDEV_MSG_TYPE_EVENT;
		msg.content.ptr = gnrc_lasmac_netdev;
		if (msg_send(&msg, gnrc_lasmac_netdev->pid) <= 0) {
		  puts("gnrc_netdev: possibly lost interrupt.");
		}
	}
	else {
		DEBUG("gnrc_netdev: event triggered -> %i\n", event);
		switch(event) {
			case NETDEV_EVENT_RX_COMPLETE: 
            {
				/* Packet decoding */
				gnrc_pktsnip_t *pkt = gnrc_lasmac_netdev->recv(gnrc_lasmac_netdev);

				if (dutycycle_state != DUTY_INIT) {
					if (dutycycle_state != DUTY_LISTEN) { /* Should be in LISTEN state */
						DEBUG("gnrc_netdev: SOMETHING IS WRONG\n");
					}
					xtimer_remove(&timer);
					/* ToDo: Additional wakeup when receiving a header with pending bit */
					radio_busy        = false; /* radio is free now */
					additional_wakeup = false; 
					
					msg_t msg;
					msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;
					msg_send(&msg, gnrc_lasmac_netdev->pid);
				}

				if (pkt) {
					_pass_on_packet(pkt);
				}
				break;
            }
			case NETDEV_EVENT_TX_COMPLETE_DATA_PENDING: /* Response for Data Request packet*/
			{
#ifdef MODULE_NETSTATS_L2
       	    	dev->stats.tx_success++;
#endif
				if (dutycycle_state != DUTY_INIT) {
					if (dutycycle_state != DUTY_TX_DATAREQ) { /* Should be in DUTY_TX_DATAREQ */
						DEBUG("gnrc_netdev: SOMETHING IS WRONG\n");
					}
					/* LISTEN for a while for packet reception */
					radio_busy        = false; /* radio is free now */
					additional_wakeup = true;  /* should wake up further */

					msg_t msg;
					msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;
					msg_send(&msg, gnrc_lasmac_netdev->pid);
				}
	       		break;
			}
			case NETDEV_EVENT_TX_COMPLETE:
			case NETDEV_EVENT_TX_MEDIUM_BUSY:
			case NETDEV_EVENT_TX_NOACK:
			{
#ifdef MODULE_NETSTATS_L2
				if (event == NETDEV_EVENT_TX_COMPLETE) {
	       	    	dev->stats.tx_success++;
				} else {
					dev->stats.tx_failed++;
				}
#endif
				if (dutycycle_state != DUTY_INIT) {
					radio_busy = false; /* radio is free now */
					msg_t msg;
					if (dutycycle_state == DUTY_TX_DATAREQ) {
						additional_wakeup = false;  /* should wake up further */
						msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;				
					} else { 
						if (pending_num) {
							msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
						} else {
							msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;				
						}
					}
					msg_send(&msg, gnrc_lasmac_netdev->pid);							
				}
				break;
			}
			default:
			DEBUG("gnrc_netdev: warning: unhandled event %u.\n", event);
		}
	}
}

static void _pass_on_packet(gnrc_pktsnip_t *pkt)
{
    /* throw away packet if no one is interested */
    if (!gnrc_netapi_dispatch_receive(pkt->type, GNRC_NETREG_DEMUX_CTX_ALL, pkt)) {
        DEBUG("gnrc_netdev: unable to forward packet of type %i\n", pkt->type);
        gnrc_pktbuf_release(pkt);
        return;
    }
}

/**
 * @brief   Startup code and event loop of the gnrc_netdev layer
 *
 * @param[in] args  expects a pointer to the underlying netdev device
 *

 * @return          never returns
 */
static void *_gnrc_lasmac_thread(void *args)
{
    DEBUG("gnrc_netdev: starting thread\n");

    gnrc_netdev_t* gnrc_lasmac_netdev = (gnrc_netdev_t*) args;
    netdev_t *dev = gnrc_lasmac_netdev->dev;
    gnrc_lasmac_netdev->pid = thread_getpid();

    timer.callback = dutycycle_cb;
    timer.arg = (void*) gnrc_lasmac_netdev;
    netopt_state_t sleepstate;
    uint16_t src_len = IEEE802154_SHORT_ADDRESS_LEN;

    gnrc_netapi_opt_t *opt;
    int res;

    /* setup the MAC layers message queue (general purpose) */
    msg_t msg, reply, msg_queue[NETDEV_NETAPI_MSG_QUEUE_SIZE];
    msg_init_queue(msg_queue, NETDEV_NETAPI_MSG_QUEUE_SIZE);

	/* setup the MAC layers packet queue (only for packet transmission) */
	msg_t pkt_queue[NETDEV_PKT_QUEUE_SIZE];
	for (int i=0; i<NETDEV_PKT_QUEUE_SIZE; i++) {
		pkt_queue[i].sender_pid = 0;
		pkt_queue[i].type = 0;
	}

    /* register the event callback with the device driver */
    dev->event_callback = _event_cb;
    dev->context = (void*) gnrc_lasmac_netdev;

    /* register the device to the network stack*/
    gnrc_netif_add(thread_getpid());

    /* initialize low-level driver */
    dev->driver->init(dev);

	/* use short address for duty-cycling */
	dev->driver->set(dev, NETOPT_SRC_LEN, &src_len, sizeof(src_len));

	/* start duty-cycle */
	if (DUTYCYCLE_SLEEP_INTERVAL) {
		/* Duty-cycle for receiving packets (listen-after-send) */
		dutycycle_state = DUTY_LISTEN;
		sleepstate = NETOPT_STATE_IDLE;
		xtimer_set(&timer, random_uint32_range(0, 1000));
	} else {
		sleepstate = NETOPT_STATE_SLEEP;
	}
	dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
	
    /* start the event loop */
    while (1) {
        DEBUG("gnrc_netdev: waiting for incoming messages\n");
        msg_receive(&msg);

        /* dispatch NETDEV and NETAPI messages */
        switch (msg.type) {
			case GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT:
				/* radio dutycycling control */
	            DEBUG("gnrc_netdev: GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT received\n");
				if (DUTYCYCLE_SLEEP_INTERVAL) {
					switch(dutycycle_state) {
						case DUTY_INIT: /* Start dutycycling from sleep state */
							dutycycle_state = DUTY_SLEEP;
							sleepstate = NETOPT_STATE_SLEEP;
							dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));	
							dev->driver->set(dev, NETOPT_SRC_LEN, &src_len, sizeof(src_len));
							xtimer_set(&timer,random_uint32_range(0, DUTYCYCLE_SLEEP_INTERVAL));
							DEBUG("gnrc_netdev: INIT DUTYCYCLE\n");					
							break;
						case DUTY_TX_DATA:  /* All data packets are sent (TRX interrupt) */
							dutycycle_state = DUTY_SLEEP;
							sleepstate = NETOPT_STATE_SLEEP;
							dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
							DEBUG("gnrc_netdev: RADIO OFF\n\n");		
							break;
						case DUTY_TX_DATA_BEFORE_DATAREQ: /* After Tx all data packets before a data request 
														   * (TRX interrupt) */
							/* Send a data request */
							dutycycle_state = DUTY_TX_DATAREQ;
							radio_busy = true; /* radio is now busy */	
							gnrc_lasmac_netdev->send_dataReq(gnrc_lasmac_netdev);
							DEBUG("gnrc_netdev: SEND DATAREQ\n");		
							break;
						case DUTY_TX_DATAREQ: /* After a data request is sent (TRX interrupt) */
						case DUTY_LISTEN:     /* After Listening is ended (timer expiry or TRX interrupt */
							if (additional_wakeup) {  /* Need to wake up further to receive more packets */
								xtimer_set(&timer, DUTYCYCLE_WAKEUP_INTERVAL);
								additional_wakeup = false;
								dutycycle_state = DUTY_LISTEN;
								sleepstate = NETOPT_STATE_IDLE;
								dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
								DEBUG("gnrc_netdev: RADIO REMAINS ON\n");
							} else if (pending_num) { /* Have some packets to send */
								xtimer_set(&timer, DUTYCYCLE_SLEEP_INTERVAL);
								dutycycle_state = DUTY_TX_DATA;
								msg_queue_send(pkt_queue, gnrc_lasmac_netdev);
								DEBUG("gnrc_netdev: RADIO OFF\n\n");
							} else {                  /* Good to fall asleep */
								xtimer_set(&timer, DUTYCYCLE_SLEEP_INTERVAL);
								dutycycle_state = DUTY_SLEEP;
								sleepstate = NETOPT_STATE_SLEEP;
								dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
								DEBUG("gnrc_netdev: RADIO OFF\n\n");
							}
							break;
						case DUTY_SLEEP: /* Wake up from the sleep (timer expiry) */
							if (pending_num) { /* Have some packets to send before data req */
								dutycycle_state = DUTY_TX_DATA_BEFORE_DATAREQ;
								msg_queue_send(pkt_queue, gnrc_lasmac_netdev);
								DEBUG("gnrc_netdev: SEND DATA BEFORE DATAREQ\n");
							} else {           /* Send a data request */
								dutycycle_state = DUTY_TX_DATAREQ;
								radio_busy = true; /* radio is now busy */	
								gnrc_lasmac_netdev->send_dataReq(gnrc_lasmac_netdev);
								DEBUG("gnrc_netdev: SEND DATAREQ\n");					
							}
							break;
						default:
							break;
					}
				} else {
					/* somthing is wrong */
					DEBUG("gnrc_netdev: SOMETHING IS WRONG\n");
				}
				break;
			case GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE:
				/* Remove a packet from the packet queue */
				msg_queue_remove_head(pkt_queue);
				/* Send a packet in the packet queue */
				if (pending_num) {
					/* Send any packet */
					msg_queue_send(pkt_queue, gnrc_lasmac_netdev);
				} else {
					if (dutycycle_state == DUTY_TX_DATA_BEFORE_DATAREQ) {
						dutycycle_state = DUTY_TX_DATAREQ;			
						radio_busy = true; /* radio is now busy */			
						gnrc_lasmac_netdev->send_dataReq(gnrc_lasmac_netdev);
						DEBUG("gnrc_netdev: SEND DATAREQ AFTER DATA\n");					
					} else {								
						dutycycle_state = DUTY_SLEEP;
						sleepstate = NETOPT_STATE_SLEEP;
						dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
						DEBUG("gnrc_netdev: RADIO OFF\n\n");					
					}
				}
				break;
          	case NETDEV_MSG_TYPE_EVENT:
		        DEBUG("gnrc_netdev: GNRC_NETDEV_MSG_TYPE_EVENT received\n");
		        dev->driver->isr(dev);
		        break;
         	case GNRC_NETAPI_MSG_TYPE_SND:
		        DEBUG("gnrc_netdev: GNRC_NETAPI_MSG_TYPE_SND received\n");
				if (dutycycle_state == DUTY_INIT) {
					gnrc_pktsnip_t *pkt = msg.content.ptr;
					gnrc_lasmac_netdev->send(gnrc_lasmac_netdev, pkt);	
					DEBUG("gnrc_netdev: SENDING IMMEDIATELY\n");						
				} else {
					if (dutycycle_state == DUTY_SLEEP && !pending_num && !radio_busy) {/* Send a packet now */
						dutycycle_state = DUTY_TX_DATA;
						radio_busy = true; /* radio is now busy */
						gnrc_pktsnip_t *pkt = msg.content.ptr;
						gnrc_lasmac_netdev->send(gnrc_lasmac_netdev, pkt);	
						DEBUG("gnrc_netdev: SENDING IMMEDIATELY %lu\n", 
								_xtimer_usec_from_ticks(timer.target - xtimer_now().ticks32));	
					} else { /* Queue a packet */
						msg_queue_add(pkt_queue, &msg);
						DEBUG("gnrc_netdev: QUEUEING %lu\n", 
								_xtimer_usec_from_ticks(timer.target - xtimer_now().ticks32));
					} 
				}
				break;
			case GNRC_NETAPI_MSG_TYPE_SET:
				/* read incoming options */
				opt = msg.content.ptr;
				DEBUG("gnrc_netdev: GNRC_NETAPI_MSG_TYPE_SET received. opt=%s\n",
					  netopt2str(opt->opt));
				/* set option for device driver */
				res = dev->driver->set(dev, opt->opt, opt->data, opt->data_len);
				DEBUG("gnrc_netdev: response of netdev->set: %i\n", res);
				/* send reply to calling thread */
				reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
				reply.content.value = (uint32_t)res;
				msg_reply(&msg, &reply);
				break;
			case GNRC_NETAPI_MSG_TYPE_GET:
				/* read incoming options */
				opt = msg.content.ptr;
				DEBUG("gnrc_netdev: GNRC_NETAPI_MSG_TYPE_GET received. opt=%s\n",
					  netopt2str(opt->opt));
				/* get option from device driver */
				res = dev->driver->get(dev, opt->opt, opt->data, opt->data_len);
				DEBUG("gnrc_netdev: response of netdev->get: %i\n", res);
				/* send reply to calling thread */
				reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
				reply.content.value = (uint32_t)res;
				msg_reply(&msg, &reply);
				break;
			default:
				DEBUG("gnrc_netdev: Unknown command %" PRIu16 "\n", msg.type);
				break;
		}
    }
    /* never reached */
    return NULL;
}


kernel_pid_t gnrc_lasmac_init(char *stack, int stacksize, char priority,
                        const char *name, gnrc_netdev_t *gnrc_netdev)
{

	kernel_pid_t res;

    /* check if given netdev device is defined and the driver is set */
    if (gnrc_netdev == NULL || gnrc_netdev->dev == NULL) {
        return -ENODEV;
    }

    /* create new gnrc_netdev thread */
    res = thread_create(stack, stacksize, priority, THREAD_CREATE_STACKTEST,
                         _gnrc_lasmac_thread, (void *)gnrc_netdev, name);

    if (res <= 0) {
        return -EINVAL;
    }

    return res;
}
#endif
