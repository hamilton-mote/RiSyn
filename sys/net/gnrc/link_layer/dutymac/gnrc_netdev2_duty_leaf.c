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
 * @brief       Glue for netdev devices to netapi (duty-cycling protocol for leaf nodes)
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
#include "net/ethernet/hdr.h"
#include "random.h"
#include "net/ieee802154.h"
#include <periph/gpio.h>


#include "xtimer.h"

#include "send.h"

#if DUTYCYCLE_EN
#if LEAF_NODE

#define ENABLE_DEBUG    (0)
#include "debug.h"

#if defined(MODULE_OD) && ENABLE_DEBUG
#include "od.h"
#endif

#define NETDEV_NETAPI_MSG_QUEUE_SIZE 16
#define NETDEV_PKT_QUEUE_SIZE 128


static uint8_t sleep_interval_shift = 0;

static void reset_sleep_interval(void) {
	int state = irq_disable();
	sleep_interval_shift = 0;
	irq_restore(state);
}
static void backoff_sleep_interval(void) {
	int state = irq_disable();
	uint32_t interval = (DUTYCYCLE_SLEEP_INTERVAL_MIN << sleep_interval_shift);
	if (interval < DUTYCYCLE_SLEEP_INTERVAL_MAX) {
		assert((interval << 1) >= interval); // check for overflow
		sleep_interval_shift++;
	}
	irq_restore(state);
}
static uint32_t get_sleep_interval(void) {
	uint32_t interval = (DUTYCYCLE_SLEEP_INTERVAL_MIN << sleep_interval_shift);
	if (interval > DUTYCYCLE_SLEEP_INTERVAL_MAX) {
		interval = DUTYCYCLE_SLEEP_INTERVAL_MAX;
	}
	return interval;
}

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

kernel_pid_t dutymac_netdev_pid;

/* A packet can be sent only when radio_busy = 0 */
bool radio_busy = false;

/* This is the packet being sent by the radio now */
gnrc_pktsnip_t *current_pkt;

bool additional_wakeup = false;

bool retry_rexmit = false;
void send_packet(gnrc_pktsnip_t* pkt, gnrc_netdev_t* gnrc_dutymac_netdev, bool retransmission) {
	retry_rexmit = retransmission;
	msg_t msg;
	msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_LINK_RETRANSMIT;
	msg.content.ptr = pkt;
	if (msg_send(&msg, dutymac_netdev_pid) <= 0) {
		assert(false);
	}
}

bool sending_beacon = false;

void send_packet_csma(gnrc_pktsnip_t* pkt, gnrc_netdev_t* gnrc_dutymac_netdev, bool retransmission) {
	send_with_csma(pkt, send_packet, gnrc_dutymac_netdev, retransmission, sending_beacon);
}

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

void msg_queue_send(msg_t* msg_queue, gnrc_netdev_t* gnrc_dutymac_netdev) {
	gnrc_pktsnip_t *pkt = msg_queue[0].content.ptr;
	radio_busy = true;
	sending_beacon = false;
	send_with_retries(pkt, -1, send_packet_csma, gnrc_dutymac_netdev, false);
}

/**
 * @brief   Function called by the dutycycle timer
 *
 * @param[in] event     type of event
 */
void dutycycle_cb(void* arg) {
	//printf("timer\n");
	gnrc_netdev_t* gnrc_dutymac_netdev = (gnrc_netdev_t*) arg;
    msg_t msg;
	/* Dutycycling state control for leaf nodes */
	msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;
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
			msg_send(&msg, gnrc_dutymac_netdev->pid);
			break;
		case DUTY_LISTEN:
			if (pending_num > 0) {
				xtimer_set(&timer, get_sleep_interval());
				dutycycle_state = DUTY_TX_DATA;
				msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_CHECK_QUEUE;
				msg_send(&msg, gnrc_dutymac_netdev->pid);
			} else {
				dutycycle_state = DUTY_SLEEP;
				msg_send(&msg, gnrc_dutymac_netdev->pid);
			}
			break;
		case DUTY_TX_DATA: /* Sleep ends while transmitting data: just state change */
			dutycycle_state = DUTY_TX_DATA_BEFORE_BEACON;
			break;
		default:
			break;
	}
}

bool irq_pending = false;

/**
 * @brief   Function called by the device driver on device events
 *
 * @param[in] event     type of event
 */
static void _event_cb(netdev_t *dev, netdev_event_t event)
{
	gnrc_netdev_t* gnrc_dutymac_netdev = (gnrc_netdev_t*)dev->context;
    if (event == NETDEV_EVENT_ISR) {
		irq_pending = true;
        msg_t msg;
        msg.type = NETDEV_MSG_TYPE_EVENT;
        msg.content.ptr = gnrc_dutymac_netdev;
        if (msg_send(&msg, gnrc_dutymac_netdev->pid) <= 0) {
            puts("gnrc_netdev: possibly lost interrupt.");
        }
    }
	else if (event == NETDEV_EVENT_RX_PENDING) {
		additional_wakeup = true;
	}
    else {
        DEBUG("gnrc_netdev: event triggered -> %i\n", event);
		bool will_retry;
        switch(event) {
            case NETDEV_EVENT_RX_COMPLETE:
                {
					/* Packet decoding */
                    gnrc_pktsnip_t *pkt = gnrc_dutymac_netdev->recv(gnrc_dutymac_netdev);

					int state = irq_disable();
					xtimer_remove(&timer);

					msg_t msg;

					if (additional_wakeup) { /* LISTEN for a while for further packet reception */
						//printf("Listen after reception...\n");
						dutycycle_state = DUTY_LISTEN;
						additional_wakeup = false;
						msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;
					} else if (pending_num == 0) { /* SLEEP now */
						//printf("Sleep after reception...\n");
						dutycycle_state = DUTY_SLEEP;
						msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;
					} else {
						//printf("Send data after reception...\n");
						xtimer_set(&timer, get_sleep_interval());
						dutycycle_state = DUTY_TX_DATA;
						msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_CHECK_QUEUE;
					}

					msg_send(&msg, gnrc_dutymac_netdev->pid);

					irq_restore(state);

					if (pkt) {
                        _pass_on_packet(pkt);
                    }
					break;
                }
            case NETDEV_EVENT_TX_COMPLETE_PENDING: /* Response for Data Request packet*/
				{
#ifdef MODULE_NETSTATS_L2
         	    	dev->stats.tx_success++;
#endif
					csma_send_succeeded();
					retry_send_succeeded();

					//printf("sent, with pending\n");

					radio_busy = false;

					/* There will be data in this sleep interval. */
					reset_sleep_interval();

					if (dutycycle_state != DUTY_INIT) {
						/* Dutycycle_state must be DUTY_TX_BEACON */
						if (dutycycle_state != DUTY_TX_BEACON) {
							DEBUG("gnrc_netdev: SOMETHING IS WRONG\n");
						}
						/* LISTEN for a while for packet reception */
						//printf("remove timer 2\n");
						xtimer_remove(&timer);
						//printf("Listening after beacon...\n");
						dutycycle_state = DUTY_LISTEN;
						msg_t msg;
						msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;
						msg_send(&msg, gnrc_dutymac_netdev->pid);
					}
			        break;
				}
            case NETDEV_EVENT_TX_COMPLETE:
				{
#ifdef MODULE_NETSTATS_L2
         	    	dev->stats.tx_success++;
#endif
					csma_send_succeeded();
					retry_send_succeeded();

					radio_busy = false; /* radio is free now */

					if (dutycycle_state != DUTY_INIT) {
						msg_t msg;
						if (dutycycle_state == DUTY_TX_BEACON) { /* Sleep again */
							//printf("remove timer 3\n");
							xtimer_remove(&timer);

							/* No data in this interval... */
							backoff_sleep_interval();

							dutycycle_state = DUTY_SLEEP;
							msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;
							msg_send(&msg, gnrc_dutymac_netdev->pid);
						} else if (pending_num > 0) { /* Remove the packet from the queue */
							/* We just sent a data-containing packet. */
							reset_sleep_interval();

							if (dutycycle_state != DUTY_TX_DATA) {
								assert(dutycycle_state != DUTY_SLEEP);
								//printf("remove timer 4\n");
								xtimer_remove(&timer);
							}
							msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
							msg_send(&msg, gnrc_dutymac_netdev->pid);
						} else if (dutycycle_state == DUTY_TX_DATA) {
							msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;
							msg_send(&msg, gnrc_dutymac_netdev->pid);
						}
					}
			        break;
				}
			case NETDEV_EVENT_TX_MEDIUM_BUSY:
#ifdef MODULE_NETSTATS_L2
	            dev->stats.tx_failed++;
#endif
				will_retry = csma_send_failed();
				if (will_retry) {
					break;
				}
                /* fallthrough intentional */
			case NETDEV_EVENT_TX_NOACK:
				if (event == NETDEV_EVENT_TX_NOACK) {
					/* CSMA succeeded... */
					csma_send_succeeded();
				}
				/* ... but the retry failed. */
				will_retry = retry_send_failed();
				if (will_retry) {
					break;
				}

				radio_busy = false;
				{
#ifdef MODULE_NETSTATS_L2
	                dev->stats.tx_failed++;
#endif
					if (dutycycle_state != DUTY_INIT) {
						msg_t msg;
						if (dutycycle_state == DUTY_TX_BEACON) { /* Sleep again */
							//printf("remove timer 5\n");
							xtimer_remove(&timer);
							dutycycle_state = DUTY_SLEEP;
							msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;
							msg_send(&msg, gnrc_dutymac_netdev->pid);
						} else if (pending_num > 0) { /* Remove the packet from the queue */
							if (dutycycle_state != DUTY_TX_DATA) {
								assert(dutycycle_state != DUTY_SLEEP);
								//printf("remove timer 6\n");
								xtimer_remove(&timer);
							}
							msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
							msg_send(&msg, gnrc_dutymac_netdev->pid);
						} else if (dutycycle_state == DUTY_TX_DATA) {
							msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;
							msg_send(&msg, gnrc_dutymac_netdev->pid);
						}
					}
					break;
				}
            default:
                DEBUG("gnrc_netdev: warning: unhandled event %u.\n", event);
        }
    }
}

static bool is_receiving(netdev_t* dev) {
	netopt_state_t state;
	int rv = dev->driver->get(dev, NETOPT_STATE, &state, sizeof(state));
	if (rv != sizeof(state)) {
		assert(false);
	}
	return state == NETOPT_STATE_RX;
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

bool beacon_pending = false;
static void send_beacon_safely(gnrc_netdev_t* gnrc_dutymac_netdev) {
	if (radio_busy || irq_pending || is_receiving(gnrc_dutymac_netdev->dev)) {
		beacon_pending = true;
	} else {
		//printf("sending beacon...\n");
		radio_busy = true;
		sending_beacon = true;
		send_with_retries(NULL, -1, send_packet_csma, gnrc_dutymac_netdev, false);
	}
}

msg_t pkt_queue[NETDEV_PKT_QUEUE_SIZE];

/**
 * @brief   Startup code and event loop of the gnrc_netdev layer
 *
 * @param[in] args  expects a pointer to the underlying netdev device
 *

 * @return          never returns
 */
static void *_gnrc_netdev_duty_thread(void *args)
{
    DEBUG("gnrc_netdev: starting thread\n");

    gnrc_netdev_t* gnrc_dutymac_netdev = (gnrc_netdev_t*) args;
    netdev_t *dev = gnrc_dutymac_netdev->dev;
    gnrc_dutymac_netdev->pid = thread_getpid();
	dutymac_netdev_pid = gnrc_dutymac_netdev->pid;

	timer.callback = dutycycle_cb;
	timer.arg = (void*) gnrc_dutymac_netdev;
	netopt_state_t sleepstate;
	uint16_t src_len = IEEE802154_SHORT_ADDRESS_LEN;

    gnrc_netapi_opt_t *opt;
    int res;

    /* setup the MAC layers message queue (general purpose) */
    msg_t msg, reply, msg_queue[NETDEV_NETAPI_MSG_QUEUE_SIZE];
    msg_init_queue(msg_queue, NETDEV_NETAPI_MSG_QUEUE_SIZE);

	/* setup the MAC layers packet queue (only for packet transmission) */
	for (int i=0; i<NETDEV_PKT_QUEUE_SIZE; i++) {
		pkt_queue[i].sender_pid = 0;
		pkt_queue[i].type = 0;
	}

    /* register the event callback with the device driver */
    dev->event_callback = _event_cb;
    dev->context = (void*) gnrc_dutymac_netdev;

    /* register the device to the network stack*/
    gnrc_netif_add(thread_getpid());

    /* initialize low-level driver */
    dev->driver->init(dev);

    /* start the event loop */
    while (1) {
        DEBUG("gnrc_netdev: waiting for incoming messages\n");
        msg_receive(&msg);

        /* dispatch NETDEV and NETAPI messages */
        switch (msg.type) {
			case GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT:
				/* radio dutycycling control */
                DEBUG("gnrc_netdev: GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT received\n");
				if (dutycycling == NETOPT_ENABLE) {
					switch(dutycycle_state) {
						case DUTY_INIT: /* Start dutycycling from sleep state */
							dutycycling = NETOPT_ENABLE;
							dutycycle_state = DUTY_SLEEP;
							sleepstate = NETOPT_STATE_SLEEP;
							dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
							dev->driver->set(dev, NETOPT_SRC_LEN, &src_len, sizeof(src_len));
							xtimer_set(&timer,random_uint32_range(0, DUTYCYCLE_SLEEP_INTERVAL_MAX));
							DEBUG("gnrc_netdev: INIT DUTYCYCLE\n");
							break;
						case DUTY_TX_BEACON: /* Tx a beacon after wake-up */
							//printf("remove timer 7\n");
							xtimer_remove(&timer);
							send_beacon_safely(gnrc_dutymac_netdev);
							DEBUG("gnrc_netdev: SEND BEACON\n");
							break;
						case DUTY_TX_DATA:  /* After Tx all data packets */
							// Timer is running in this state... when it expires we move to DUTY_TX_DATA_BEFORE_BEACON
							dutycycle_state = DUTY_SLEEP;
							sleepstate = NETOPT_STATE_SLEEP;
							dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
							DEBUG("gnrc_netdev: RADIO OFF\n\n");
							break;
						case DUTY_TX_DATA_BEFORE_BEACON:
							//printf("remove timer 8\n");
							xtimer_remove(&timer);
							if (!radio_busy && !irq_pending && !is_receiving(dev)) {
								msg_queue_send(pkt_queue, gnrc_dutymac_netdev);
							}
							DEBUG("gnrc_netdev: SEND DATA BEFORE BEACON\n");
							break;
						case DUTY_LISTEN: /* Idle listening after transmission or reception */
							//radio_busy = false;
							dev->driver->get(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
							sleepstate = NETOPT_STATE_IDLE;
							dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
							//printf("wakeup timer set\n");
							xtimer_set(&timer, DUTYCYCLE_WAKEUP_INTERVAL);
							DEBUG("gnrc_netdev: RADIO REMAINS ON\n");
							break;
						case DUTY_SLEEP: /* Go to sleep */
							//gpio_write(GPIO_PIN(0,19),0);
							sleepstate = NETOPT_STATE_SLEEP;
							dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
							xtimer_set(&timer, get_sleep_interval());
							DEBUG("gnrc_netdev: RADIO OFF\n\n");
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
					if (!radio_busy && !irq_pending && !is_receiving(dev)) {
						/* Send any packet */
						msg_queue_send(pkt_queue, gnrc_dutymac_netdev);
					}
				} else {
					if (dutycycle_state == DUTY_TX_DATA_BEFORE_BEACON) {
						dutycycle_state = DUTY_TX_BEACON;
						send_beacon_safely(gnrc_dutymac_netdev);
						DEBUG("gnrc_netdev: SEND BEACON AFTER DATA\n");
					} else if (dutycycle_state == DUTY_TX_DATA) {
						/*msg_t nmsg;
						nmsg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_EVENT;
						msg_send_to_self(&nmsg);*/
						//printf("Emptied send queue, going to sleep...\n");
						dutycycle_state = DUTY_SLEEP;
						sleepstate = NETOPT_STATE_SLEEP;
						dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
						DEBUG("gnrc_netdev: RADIO OFF\n\n");
					}
				}
				break;
			case GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_CHECK_QUEUE:
				if (dutycycle_state != DUTY_LISTEN && pending_num != 0 && !radio_busy && !irq_pending && !is_receiving(dev)) {
					if (dutycycle_state == DUTY_SLEEP) {
						dutycycle_state = DUTY_TX_DATA;
					}
					msg_queue_send(pkt_queue, gnrc_dutymac_netdev);
				}
				break;
            case NETDEV_MSG_TYPE_EVENT:
                DEBUG("gnrc_netdev: GNRC_NETDEV_MSG_TYPE_EVENT received\n");
				irq_pending = false;
                dev->driver->isr(dev);
				if (beacon_pending && !radio_busy) {
					//printf("sending pending beacon\n");
					beacon_pending = false;
					radio_busy = true;
					sending_beacon = true;
					send_with_retries(NULL, -1, send_packet_csma, gnrc_dutymac_netdev, false);
				}
				{
					msg_t nmsg;
					nmsg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_CHECK_QUEUE;
					msg_send_to_self(&nmsg);
				}
                break;
            case GNRC_NETAPI_MSG_TYPE_SND:
                DEBUG("gnrc_netdev: GNRC_NETAPI_MSG_TYPE_SND received\n");
				/* Queue it no matter what. */
				msg_queue_add(pkt_queue, &msg);
				if (dutycycle_state == DUTY_INIT) {
					msg_queue_send(pkt_queue, gnrc_dutymac_netdev);
					DEBUG("gnrc_netdev: SENDING IMMEDIATELY %lu\n");
				} else {
					if (/*_xtimer_usec_from_ticks(timer.target - xtimer_now().ticks32) < 50000 ||*/
						pending_num > 1 || radio_busy) {
						DEBUG("gnrc_netdev: QUEUEING %lu\n", _xtimer_usec_from_ticks(timer.target - xtimer_now().ticks32));
					} else if (!radio_busy && !irq_pending && !is_receiving(dev) && dutycycle_state == DUTY_SLEEP){
						/* Send a packet now */
						dutycycle_state = DUTY_TX_DATA;
						msg_queue_send(pkt_queue, gnrc_dutymac_netdev);
						DEBUG("gnrc_netdev: SENDING IMMEDIATELY %lu\n", _xtimer_usec_from_ticks(timer.target - xtimer_now().ticks32));
					}
				}
		        break;
            case GNRC_NETAPI_MSG_TYPE_SET:
                /* read incoming options */
                opt = msg.content.ptr;
                DEBUG("gnrc_netdev: GNRC_NETAPI_MSG_TYPE_SET received. opt=%s\n",
                        netopt2str(opt->opt));
				if (opt->opt == NETOPT_DUTYCYCLE) {
					dutycycling = *(netopt_enable_t*) opt->data;
					//printf("remove timer 9\n");
					xtimer_remove(&timer);
					if (dutycycling == NETOPT_ENABLE) {
						/* Dutycycle start triggered by application layer */
						dutycycle_state = DUTY_SLEEP;
						sleepstate = NETOPT_STATE_SLEEP;
						xtimer_set(&timer, random_uint32_range(0, DUTYCYCLE_SLEEP_INTERVAL_MAX));
						DEBUG("gnrc_netdev: INIT DUTYCYCLE\n");
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
			case GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_LINK_RETRANSMIT:
				if (!irq_pending && !is_receiving(dev)) {
					if (sending_beacon) {
						res = gnrc_dutymac_netdev->send_beacon(gnrc_dutymac_netdev);
					} else {
						if (retry_rexmit) {
							res = gnrc_dutymac_netdev->resend_without_release(gnrc_dutymac_netdev, msg.content.ptr, false);
						} else {
							res = gnrc_dutymac_netdev->send_without_release(gnrc_dutymac_netdev, msg.content.ptr, false);
						}
					}
					if (res < 0) {
						_event_cb(dev, NETDEV_EVENT_TX_MEDIUM_BUSY);
					}
				} else {
					msg_t nmsg;
					nmsg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_LINK_RETRANSMIT;
					nmsg.content.ptr = msg.content.ptr;
					msg_send_to_self(&nmsg);
				}
            default:
                DEBUG("gnrc_netdev: Unknown command %" PRIu16 "\n", msg.type);
                break;
        }
    }
    /* never reached */
    return NULL;
}


kernel_pid_t gnrc_netdev_dutymac_init(char *stack, int stacksize, char priority,
                        const char *name, gnrc_netdev_t *gnrc_netdev)
{

	kernel_pid_t res;

	retry_init();
	csma_init();

    /* check if given netdev device is defined and the driver is set */
    if (gnrc_netdev == NULL || gnrc_netdev->dev == NULL) {
        return -ENODEV;
    }

    /* create new gnrc_netdev thread */
    res = thread_create(stack, stacksize, priority, THREAD_CREATE_STACKTEST,
                         _gnrc_netdev_duty_thread, (void *)gnrc_netdev, name);

    if (res <= 0) {
        return -EINVAL;
    }

    return res;
}
#endif
#endif
