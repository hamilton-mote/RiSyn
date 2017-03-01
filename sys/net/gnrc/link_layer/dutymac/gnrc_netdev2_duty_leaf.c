/*
 * Copyright (C) 2015 Freie Universität Berlin
 *               2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net
 * @file
 * @brief       Glue for netdev2 devices to netapi (duty-cycling protocol for leaf nodes)
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
#include "net/netdev2.h"

#include "net/gnrc/netdev2.h"
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

#define NETDEV2_NETAPI_MSG_QUEUE_SIZE 8
#define NETDEV2_PKT_QUEUE_SIZE 4
#define DUTYCYCLE_WAKEUP_INTERVAL  6000UL    /* Don't change it w/o particular reasons */

static void _pass_on_packet(gnrc_pktsnip_t *pkt);

/** 1) For a leaf node (battery-powered), 'dutycycling' is set to NETOPT_ENABLE by application
 *  2) For a router (wall-powered), 'dutycycling' remains to NETOPT_DISABLE
 */
netopt_enable_t   dutycycling     = NETOPT_DISABLE;

/*  Dutycycle state (INIT, SLEEP, TXBEACON, TXDATA, and LISTEN) */
typedef enum {
	DUTY_INIT,
	DUTY_SLEEP,
	DUTY_TX_BEACON,
	DUTY_TX_DATA,
	DUTY_TX_DATA_BEFORE_BEACON,
	DUTY_LISTEN,
} dutycycle_state_t;
dutycycle_state_t dutycycle_state = DUTY_INIT;

/**	1) For a leaf node, 'timer' is used for wake-up scheduling
 *  2) For a router, 'timer' is used for broadcasting;
 *     a router does not discard a broadcasting packet during a sleep interval
 */
xtimer_t timer;
uint8_t pending_num = 0;

/* A packet can be sent only when radio_busy = 0 */
uint8_t radio_busy = 0;

/* This is the packet being sent by the radio now */
gnrc_pktsnip_t *current_pkt;

bool additional_wakeup = false;

// FIFO QUEUE
int msg_queue_add(msg_t* msg_queue, msg_t* msg) {
	if (pending_num < NETDEV2_PKT_QUEUE_SIZE) {
		/* Add a packet to the last entry of the queue */
		msg_queue[pending_num].sender_pid = msg->sender_pid;
		msg_queue[pending_num].type = msg->type;
		msg_queue[pending_num].content.ptr = msg->content.ptr;
		DEBUG("\nqueue add success [%u/%u/%4x]\n", pending_num, msg_queue[pending_num].sender_pid,
				msg_queue[pending_num].type);
		pending_num++; /* Number of packets in the queue */
		return 1;
	} else {
		DEBUG("Queue loss at netdev2\n");
		return 0;
	}
}

void msg_queue_remove_head(msg_t* msg_queue) {
	/* Remove queue head */
	DEBUG("remove queue [%u]\n", pending_num-1);
	gnrc_pktbuf_release(msg_queue[0].content.ptr);
	pending_num--;
	if (pending_num < 0) {
		DEBUG("NETDEV2: Pending number error\n");
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

void msg_queue_send(msg_t* msg_queue, gnrc_netdev2_t* gnrc_dutymac_netdev2) {
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
	radio_busy = 1; /* radio is now busy */
	gnrc_dutymac_netdev2->send(gnrc_dutymac_netdev2, current_pkt);
}


/**
 * @brief   Function called by the dutycycle timer
 *
 * @param[in] event     type of event
 */
void dutycycle_cb(void* arg) {
	gnrc_netdev2_t* gnrc_dutymac_netdev2 = (gnrc_netdev2_t*) arg;
    msg_t msg;
	/* Dutycycling state control for leaf nodes */
	msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_EVENT;
	switch(dutycycle_state) {
		case DUTY_INIT:
			break;
		case DUTY_SLEEP:
			//gpio_write(GPIO_PIN(0,19),1);
			if (pending_num) {
				dutycycle_state = DUTY_TX_DATA_BEFORE_BEACON;
			} else {
				dutycycle_state = DUTY_TX_BEACON;
			}
			msg_send(&msg, gnrc_dutymac_netdev2->pid);
			break;
		case DUTY_LISTEN:
			dutycycle_state = DUTY_SLEEP;
			msg_send(&msg, gnrc_dutymac_netdev2->pid);
			break;
		case DUTY_TX_DATA: /* Sleep ends while transmitting data: just state change */
			dutycycle_state = DUTY_TX_DATA_BEFORE_BEACON;
			break;
		default:
			break;
	}
}

/**
 * @brief   Function called by the device driver on device events
 *
 * @param[in] event     type of event
 */
static void _event_cb(netdev2_t *dev, netdev2_event_t event)
{
	gnrc_netdev2_t* gnrc_dutymac_netdev2 = (gnrc_netdev2_t*)dev->context;
    if (event == NETDEV2_EVENT_ISR) {
        msg_t msg;
        msg.type = NETDEV2_MSG_TYPE_EVENT;
        msg.content.ptr = gnrc_dutymac_netdev2;
        if (msg_send(&msg, gnrc_dutymac_netdev2->pid) <= 0) {
            puts("gnrc_netdev2: possibly lost interrupt.");
        }
    }
	else if (event == NETDEV2_EVENT_RX_PENDING) {
		additional_wakeup = true;
	}
    else {
        DEBUG("gnrc_netdev2: event triggered -> %i\n", event);
        switch(event) {
            case NETDEV2_EVENT_RX_COMPLETE:
                {
					xtimer_remove(&timer);
					/* Packet decoding */
                    gnrc_pktsnip_t *pkt = gnrc_dutymac_netdev2->recv(gnrc_dutymac_netdev2);

					if (additional_wakeup) { /* LISTEN for a while for further packet reception */
						dutycycle_state = DUTY_LISTEN;
						additional_wakeup = false;
					} else { /* SLEEP now */
						dutycycle_state = DUTY_SLEEP;
					}
					msg_t msg;
					msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_EVENT;
					msg_send(&msg, gnrc_dutymac_netdev2->pid);

					if (pkt) {
                        _pass_on_packet(pkt);
                    }
					break;
                }
            case NETDEV2_EVENT_TX_MEDIUM_BUSY:
#ifdef MODULE_NETSTATS_L2
                dev->stats.tx_failed++;
#endif
				radio_busy = 0; /* radio is free now */
                break;
            case NETDEV2_EVENT_TX_COMPLETE_PENDING: /* Response for Data Request packet*/
				{
#ifdef MODULE_NETSTATS_L2
         	    	dev->stats.tx_success++;
#endif
					if (dutycycle_state != DUTY_INIT) {
						/* Dutycycle_state must be DUTY_TX_BEACON */
						if (dutycycle_state != DUTY_TX_BEACON) {
							DEBUG("gnrc_netdev2: SOMETHING IS WRONG\n");
						}
						/* LISTEN for a while for packet reception */
						radio_busy = 0; /* radio is free now */
						xtimer_remove(&timer);
						dutycycle_state = DUTY_LISTEN;
						msg_t msg;
						msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_EVENT;
						msg_send(&msg, gnrc_dutymac_netdev2->pid);
					}
			        break;
				}
            case NETDEV2_EVENT_TX_COMPLETE:
				{
#ifdef MODULE_NETSTATS_L2
         	    	dev->stats.tx_success++;
#endif
					if (dutycycle_state != DUTY_INIT) {
						radio_busy = 0; /* radio is free now */
						msg_t msg;
						if (dutycycle_state == DUTY_TX_BEACON) { /* Sleep again */
							xtimer_remove(&timer);											
							dutycycle_state = DUTY_SLEEP;
							msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_EVENT;
							msg_send(&msg, gnrc_dutymac_netdev2->pid);							
						} else if (dutycycle_state == DUTY_TX_DATA && !pending_num) {
							msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_EVENT;				
							msg_send(&msg, gnrc_dutymac_netdev2->pid);							
						} else if (pending_num) { /* Remove the packet from the queue */
							xtimer_remove(&timer);											
							msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
							msg_send(&msg, gnrc_dutymac_netdev2->pid);							
						}
					}
			        break;
				}
			case NETDEV2_EVENT_TX_NOACK:
				{
#ifdef MODULE_NETSTATS_L2
	                dev->stats.tx_failed++;
#endif
					if (dutycycle_state != DUTY_INIT) {
						radio_busy = 0; /* radio is free now */
						msg_t msg;
						if (dutycycle_state == DUTY_TX_BEACON) { /* Sleep again */
							xtimer_remove(&timer);											
							dutycycle_state = DUTY_SLEEP;
							msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_EVENT;
							msg_send(&msg, gnrc_dutymac_netdev2->pid);							
						} else if (dutycycle_state == DUTY_TX_DATA && !pending_num) {
							msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_EVENT;
							msg_send(&msg, gnrc_dutymac_netdev2->pid);							
						} else if (pending_num) { /* Remove the packet from the queue */
							xtimer_remove(&timer);											
							msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
							msg_send(&msg, gnrc_dutymac_netdev2->pid);							
						}
					}
					break;
				}
            default:
                DEBUG("gnrc_netdev2: warning: unhandled event %u.\n", event);
        }
    }
}

static void _pass_on_packet(gnrc_pktsnip_t *pkt)
{
    /* throw away packet if no one is interested */
    if (!gnrc_netapi_dispatch_receive(pkt->type, GNRC_NETREG_DEMUX_CTX_ALL, pkt)) {
        DEBUG("gnrc_netdev2: unable to forward packet of type %i\n", pkt->type);
        gnrc_pktbuf_release(pkt);
        return;
    }
}

/**
 * @brief   Startup code and event loop of the gnrc_netdev2 layer
 *
 * @param[in] args  expects a pointer to the underlying netdev device
 *

 * @return          never returns
 */
static void *_gnrc_netdev2_duty_thread(void *args)
{
    DEBUG("gnrc_netdev2: starting thread\n");

    gnrc_netdev2_t* gnrc_dutymac_netdev2 = (gnrc_netdev2_t*) args;
    netdev2_t *dev = gnrc_dutymac_netdev2->dev;
    gnrc_dutymac_netdev2->pid = thread_getpid();

	timer.callback = dutycycle_cb;
	timer.arg = (void*) gnrc_dutymac_netdev2;
	netopt_state_t sleepstate;
	uint16_t src_len = IEEE802154_SHORT_ADDRESS_LEN;

    gnrc_netapi_opt_t *opt;
    int res;

    /* setup the MAC layers message queue (general purpose) */
    msg_t msg, reply, msg_queue[NETDEV2_NETAPI_MSG_QUEUE_SIZE];
    msg_init_queue(msg_queue, NETDEV2_NETAPI_MSG_QUEUE_SIZE);

	/* setup the MAC layers packet queue (only for packet transmission) */
	msg_t pkt_queue[NETDEV2_PKT_QUEUE_SIZE];
	for (int i=0; i<NETDEV2_PKT_QUEUE_SIZE; i++) {
		pkt_queue[i].sender_pid = 0;
		pkt_queue[i].type = 0;
	}

    /* register the event callback with the device driver */
    dev->event_callback = _event_cb;
    dev->context = (void*) gnrc_dutymac_netdev2;

    /* register the device to the network stack*/
    gnrc_netif_add(thread_getpid());

    /* initialize low-level driver */
    dev->driver->init(dev);

    /* start the event loop */
    while (1) {
        DEBUG("gnrc_netdev2: waiting for incoming messages\n");
        msg_receive(&msg);

        /* dispatch NETDEV and NETAPI messages */
        switch (msg.type) {
			case GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_EVENT:
				/* radio dutycycling control */
                DEBUG("gnrc_netdev2: GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT received\n");
				if (dutycycling == NETOPT_ENABLE) {
					switch(dutycycle_state) {
						case DUTY_INIT: /* Start dutycycling from sleep state */
							dutycycling = NETOPT_ENABLE;
							dutycycle_state = DUTY_SLEEP;
							sleepstate = NETOPT_STATE_SLEEP;
							dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));	
							dev->driver->set(dev, NETOPT_SRC_LEN, &src_len, sizeof(src_len));
							xtimer_set(&timer,random_uint32_range(0, DUTYCYCLE_SLEEP_INTERVAL));
							DEBUG("gnrc_netdev2: INIT DUTYCYCLE\n");					
							break;
						case DUTY_TX_BEACON: /* Tx a beacon after wake-up */
							gnrc_dutymac_netdev2->send_beacon(gnrc_dutymac_netdev2);
							DEBUG("gnrc_netdev2: SEND BEACON\n");					
							break;
						case DUTY_TX_DATA:  /* After Tx all data packets */
							dutycycle_state = DUTY_SLEEP;
							sleepstate = NETOPT_STATE_SLEEP;
							dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
							DEBUG("gnrc_netdev2: RADIO OFF\n\n");		
							break;
						case DUTY_TX_DATA_BEFORE_BEACON:
							msg_queue_send(pkt_queue, gnrc_dutymac_netdev2);
							DEBUG("gnrc_netdev2: SEND DATA BEFORE BEACON\n");
							break;
						case DUTY_LISTEN: /* Idle listening after transmission or reception */
							dev->driver->get(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
							sleepstate = NETOPT_STATE_IDLE;
							dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
							xtimer_set(&timer, DUTYCYCLE_WAKEUP_INTERVAL);
							DEBUG("gnrc_netdev2: RADIO REMAINS ON\n");
							break;
						case DUTY_SLEEP: /* Go to sleep */
							//gpio_write(GPIO_PIN(0,19),0);
							sleepstate = NETOPT_STATE_SLEEP;
							dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
							xtimer_set(&timer, DUTYCYCLE_SLEEP_INTERVAL);
							DEBUG("gnrc_netdev2: RADIO OFF\n\n");
							break;
						default:
							break;
					}
				} else {
					/* somthing is wrong */
					DEBUG("gnrc_netdev2: SOMETHING IS WRONG\n");
				}
				break;
			case GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE:
				/* Remove a packet from the packet queue */
				msg_queue_remove_head(pkt_queue);
				/* Send a packet in the packet queue */
				if (pending_num) {
					/* Send any packet */
					msg_queue_send(pkt_queue, gnrc_dutymac_netdev2);
				} else {
					if (dutycycle_state == DUTY_TX_DATA_BEFORE_BEACON) {
						dutycycle_state = DUTY_TX_BEACON;						
						gnrc_dutymac_netdev2->send_beacon(gnrc_dutymac_netdev2);
						DEBUG("gnrc_netdev2: SEND BEACON AFTER DATA\n");					
					} else {
						dutycycle_state = DUTY_SLEEP;
						sleepstate = NETOPT_STATE_SLEEP;
						dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
						DEBUG("gnrc_netdev2: RADIO OFF\n\n");					
					}
				}
				break;
            case NETDEV2_MSG_TYPE_EVENT:
                DEBUG("gnrc_netdev2: GNRC_NETDEV_MSG_TYPE_EVENT received\n");
                dev->driver->isr(dev);
                break;
            case GNRC_NETAPI_MSG_TYPE_SND:
                DEBUG("gnrc_netdev2: GNRC_NETAPI_MSG_TYPE_SND received\n");
				if (dutycycle_state == DUTY_INIT) {
					gnrc_pktsnip_t *pkt = msg.content.ptr;
					gnrc_dutymac_netdev2->send(gnrc_dutymac_netdev2, pkt);	
					DEBUG("gnrc_netdev2: SENDING IMMEDIATELY %lu\n");						
				} else {
					if (_xtimer_usec_from_ticks(timer.target - xtimer_now().ticks32) < 50000 || 
						pending_num || radio_busy) {
						/* Queue a packet */
						msg_queue_add(pkt_queue, &msg);
						DEBUG("gnrc_netdev2: QUEUEING %lu\n", _xtimer_usec_from_ticks(timer.target - xtimer_now().ticks32));
					} else {
						/* Send a packet now */
						dutycycle_state = DUTY_TX_DATA;
						radio_busy = 1; /* radio is now busy */
						gnrc_pktsnip_t *pkt = msg.content.ptr;
						gnrc_dutymac_netdev2->send(gnrc_dutymac_netdev2, pkt);	
						DEBUG("gnrc_netdev2: SENDING IMMEDIATELY %lu\n", _xtimer_usec_from_ticks(timer.target - xtimer_now().ticks32));	
					}
				}
		        break;
            case GNRC_NETAPI_MSG_TYPE_SET:
                /* read incoming options */
                opt = msg.content.ptr;
                DEBUG("gnrc_netdev2: GNRC_NETAPI_MSG_TYPE_SET received. opt=%s\n",
                        netopt2str(opt->opt));
				if (opt->opt == NETOPT_DUTYCYCLE) {
					dutycycling = *(netopt_enable_t*) opt->data;
					xtimer_remove(&timer);
					if (dutycycling == NETOPT_ENABLE) {
						/* Dutycycle start triggered by application layer */
						dutycycle_state = DUTY_SLEEP;
						sleepstate = NETOPT_STATE_SLEEP;
						xtimer_set(&timer, random_uint32_range(0, DUTYCYCLE_SLEEP_INTERVAL));
						DEBUG("gnrc_netdev2: INIT DUTYCYCLE\n");											
					} else {
						/* Dutycycle end triggered by application layer */
						dutycycle_state = DUTY_INIT;
						sleepstate = NETOPT_STATE_SLEEP;
					}
					/* We use short address for duty-cycling */
					dev->driver->set(dev, NETOPT_SRC_LEN, &src_len, sizeof(src_len));
					opt->opt = NETOPT_STATE;
					opt->data = &sleepstate;
				}
                /* set option for device driver */
                res = dev->driver->set(dev, opt->opt, opt->data, opt->data_len);
                DEBUG("gnrc_netdev2: response of netdev->set: %i\n", res);
                /* send reply to calling thread */
                reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
                reply.content.value = (uint32_t)res;
                msg_reply(&msg, &reply);
                break;
            case GNRC_NETAPI_MSG_TYPE_GET:
                /* read incoming options */
                opt = msg.content.ptr;
                DEBUG("gnrc_netdev2: GNRC_NETAPI_MSG_TYPE_GET received. opt=%s\n",
                        netopt2str(opt->opt));
                /* get option from device driver */
                res = dev->driver->get(dev, opt->opt, opt->data, opt->data_len);
                DEBUG("gnrc_netdev2: response of netdev->get: %i\n", res);
                /* send reply to calling thread */
                reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
                reply.content.value = (uint32_t)res;
                msg_reply(&msg, &reply);
                break;
            default:
                DEBUG("gnrc_netdev2: Unknown command %" PRIu16 "\n", msg.type);
                break;
        }
    }
    /* never reached */
    return NULL;
}


kernel_pid_t gnrc_netdev2_dutymac_init(char *stack, int stacksize, char priority,
                        const char *name, gnrc_netdev2_t *gnrc_netdev2)
{

	kernel_pid_t res;

    /* check if given netdev device is defined and the driver is set */
    if (gnrc_netdev2 == NULL || gnrc_netdev2->dev == NULL) {
        return -ENODEV;
    }

    /* create new gnrc_netdev2 thread */
    res = thread_create(stack, stacksize, priority, THREAD_CREATE_STACKTEST,
                         _gnrc_netdev2_duty_thread, (void *)gnrc_netdev2, name);

    if (res <= 0) {
        return -EINVAL;
    }

    return res;
}
#endif
