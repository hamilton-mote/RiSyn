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
 * @brief       Glue for netdev devices to netapi (duty-cycling protocol for routers)
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
#include "xtimer.h"

#include "send.h"

#if DUTYCYCLE_EN
#if ROUTER

#define ENABLE_DEBUG    (0)
#include "debug.h"

#if defined(MODULE_OD) && ENABLE_DEBUG
#include "od.h"
#endif

#define ENABLE_BROADCAST_QUEUEING 0

#define NETDEV_NETAPI_MSG_QUEUE_SIZE 8
#define NETDEV_PKT_QUEUE_SIZE 64

#define NEIGHBOR_TABLE_SIZE 10
typedef struct {
	uint16_t addr;
	uint16_t dutycycle;
	int8_t rssi;
	uint8_t lqi;
	uint8_t etx;
} link_neighbor_table_t;
link_neighbor_table_t neighbor_table[NEIGHBOR_TABLE_SIZE];
uint8_t neighbor_num = 0;

static void _pass_on_packet(gnrc_pktsnip_t *pkt);

/**	1) For a leaf node, 'timer' is used for wake-up scheduling
 *  2) For a router, 'timer' is used for broadcasting;
 *     a router does not discard a broadcasting packet during a sleep interval
 */
xtimer_t timer;
bool broadcasting = false;
uint8_t pending_num = 0;
uint8_t broadcasting_num = 0;
uint8_t sending_pkt_key = 0xFF;
/** [This is for bursty transmission.]
 *  After a router sends a packet, if it has another packet to send to the same destination
 *  (=recent_dst_l2addr), it does not have to wait for another sleep interval but sends immediately
 *	To this end, a leaf node wakes up for a while after transmitting or receiving a packet.
 */
uint16_t recent_dst_l2addr = 0;

/* A packet can be sent only when radio_busy = 0 */
bool radio_busy = false;

/* This is the packet being sent by the radio now */


/* Rx data request command from a leaf node: I can send data to the leaf node */
bool rx_data_request = false;

kernel_pid_t dutymac_netdev_pid;

/* TODO this should take a MAC address and return whether that is a duty-cycled
 * node sending beacons to this router. For now, just hardcode to true or false.
 */
static bool addr_is_dutycycled(uint16_t addr) {
	(void) addr;
	return false;
}

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

void send_packet_csma(gnrc_pktsnip_t* pkt, gnrc_netdev_t* gnrc_dutymac_netdev, bool retransmission) {
	send_with_csma(pkt, send_packet, gnrc_dutymac_netdev, retransmission, false);
}

// Exhaustive search version
int msg_queue_add(msg_t* msg_queue, msg_t* msg, gnrc_netdev_t* gnrc_dutymac_netdev) {
	if (pending_num < NETDEV_PKT_QUEUE_SIZE) {
		gnrc_pktsnip_t *pkt = msg->content.ptr;
		gnrc_netif_hdr_t* hdr = pkt->data;

		// 1) Broadcasting packet (Insert head of the queue)
		if (hdr->flags & (GNRC_NETIF_HDR_FLAGS_BROADCAST | GNRC_NETIF_HDR_FLAGS_MULTICAST)) {
#if ENABLE_BROADCAST_QUEUEING
			(void) gnrc_dutymac_netdev;
			if (broadcasting_num < pending_num) {
				for (int i=pending_num-1; i>= broadcasting_num; i--) {
					msg_queue[i+1].sender_pid = msg_queue[i].sender_pid;
					msg_queue[i+1].type = msg_queue[i].type;
					msg_queue[i+1].content.ptr = msg_queue[i].content.ptr;
				}
			}
			msg_queue[broadcasting_num].sender_pid = msg->sender_pid;
			msg_queue[broadcasting_num].type = msg->type;
			msg_queue[broadcasting_num].content.ptr = msg->content.ptr;

			/** When it is the first and broadcasting packet and the nodes is a router,
			  * MAC maintains the packet for a sleep interval to send it to all neighbors
			  */
			if (broadcasting_num == 0) {
				xtimer_set(&timer, DUTYCYCLE_SLEEP_INTERVAL+100);
				broadcasting = true;
				sending_pkt_key = 0;
				printf("broadcast starts\n");
			}
			broadcasting_num++;
#else
			/* Send it right away. */
			if (!radio_busy) {
				radio_busy = true;
				msg_queue[pending_num].sender_pid = msg->sender_pid;
				msg_queue[pending_num].type = msg->type;
				msg_queue[pending_num].content.ptr = msg->content.ptr;
				sending_pkt_key = pending_num;
				pending_num++;
				send_with_retries(pkt, 0, send_packet_csma, gnrc_dutymac_netdev, false);
				return 1;
			}
			return 0;
#endif
		}
		// 2) Unicasting packet
		else {
			/* Add a packet to the last entry of the queue */
			msg_queue[pending_num].sender_pid = msg->sender_pid;
			msg_queue[pending_num].type = msg->type;
			msg_queue[pending_num].content.ptr = msg->content.ptr;
			DEBUG("\nqueue add success [%u/%u/%4x]\n", pending_num, msg_queue[pending_num].sender_pid,
					msg_queue[pending_num].type);
		}
		pending_num++; /* Number of packets in the queue */
		return 1;
	} else {
		DEBUG("Queue loss at netdev\n");
		return 0;
	}
}

void msg_queue_remove(msg_t* msg_queue) {
	/* Remove a sent packet from MAC queue */
	if (sending_pkt_key == 0xFF)
		return;

	DEBUG("NETDEV: Remove queue [%u, %u/%u]\n", sending_pkt_key, broadcasting_num, pending_num-1);

	gnrc_pktbuf_release(msg_queue[sending_pkt_key].content.ptr);
	pending_num--;
	if (pending_num < 0) {
		DEBUG("NETDEV: Pending number error\n");
	}

    /* Update queue when more pending packets exist */
	if (pending_num) {
		for (int i=sending_pkt_key; i<pending_num; i++) {
			msg_queue[i].sender_pid = msg_queue[i+1].sender_pid;
			msg_queue[i].type = msg_queue[i+1].type;
			msg_queue[i].content.ptr = msg_queue[i+1].content.ptr;
			if (msg_queue[i].sender_pid == 0 && msg_queue[i].type == 0) {
				break;
			}
		}

		/** When the next packet is a broadcasting packet and the node is a router,
		  * MAC maintains the packet for a sleep interval to send it to all neighbors
		  */
		if (broadcasting_num > 0) {
			xtimer_set(&timer, DUTYCYCLE_SLEEP_INTERVAL+100);
			broadcasting = true;
			sending_pkt_key = 0;
			printf("broadcast starts\n");
			return;
		}
	}
	sending_pkt_key = 0xFF;
	return;
}

/* If to_dutycycled_dest is true, then we know that a dutycycled node is listening and
 * are trying to find packets destined for that node.
 * If to_dutycycled_dest is false, then we are looking for packets destined for a
 * neighboring always-on node.
 */
void msg_queue_send(msg_t* msg_queue, bool to_dutycycled_dest, uint16_t dst_l2addr, gnrc_netdev_t* gnrc_dutymac_netdev) {
	gnrc_pktsnip_t *pkt = NULL;

	if (broadcasting) { // broadcasting
		pkt = msg_queue[0].content.ptr;
		sending_pkt_key = 0;
		recent_dst_l2addr = 0xFFFF;

	} else {  // unicasting
		gnrc_pktsnip_t *temp_pkt;
		gnrc_netif_hdr_t *temp_hdr;
		uint16_t pkt_dst_l2addr;
		uint8_t* dst;
		for (int i=0; i<pending_num; i++) {
			temp_pkt = msg_queue[i].content.ptr;
			temp_hdr = temp_pkt->data;
			dst = gnrc_netif_hdr_get_dst_addr(temp_hdr);
			if (temp_hdr->dst_l2addr_len == IEEE802154_SHORT_ADDRESS_LEN) {
				pkt_dst_l2addr = (((uint16_t) dst[1]) << 8) | (uint16_t) dst[0];
			} else {
				pkt_dst_l2addr = (((uint16_t) dst[7]) << 8) | (uint16_t) dst[6];
			}

			if ((to_dutycycled_dest && pkt_dst_l2addr == dst_l2addr) || (!to_dutycycled_dest && !addr_is_dutycycled(pkt_dst_l2addr))) {
				pkt = msg_queue[i].content.ptr;
				recent_dst_l2addr = pkt_dst_l2addr;
				sending_pkt_key = i;
				break;
			}
		}
	}

	assert(!radio_busy);

	if (pkt != NULL && sending_pkt_key != 0xFF) {
		//printf("sending %u to %4x (%u/%u)\n", sending_pkt_key, recent_dst_l2addr, broadcasting_num, pending_num);

		radio_busy = true; /* radio is now busy */
		//send_packet(pkt, gnrc_dutymac_netdev);
		//send_with_retries(pkt, send_packet, gnrc_dutymac_netdev, false);
		send_with_retries(pkt, -1, send_packet_csma, gnrc_dutymac_netdev, false);
	}
}

/**
 * @brief   Function called by the broadcast timer
 *
 * @param[in] event     type of event
 */
void broadcast_cb(void* arg) {
	gnrc_netdev_t* gnrc_dutymac_netdev = (gnrc_netdev_t*) arg;
    msg_t msg;
	/* Broadcasting msg maintenance for routers */
	broadcasting = false;
	broadcasting_num--;
	printf("braodcast ends\n");
	msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
	msg_send(&msg, gnrc_dutymac_netdev->pid);
}

void neighbor_table_update(uint16_t l2addr, gnrc_netif_hdr_t *hdr) {
	uint8_t key = 0xFF;
	for (int8_t i=0; i<NEIGHBOR_TABLE_SIZE; i++) {
		if (neighbor_table[i].addr == l2addr) {
			key = i;
			break;
		}
	}
	if (key == 0xFF) {
		key = neighbor_num;
		neighbor_table[neighbor_num].addr = l2addr;
		neighbor_table[neighbor_num].rssi = -94 + 3*hdr->rssi; /* when using AT86RF233 transceiver*/
		neighbor_table[neighbor_num].lqi  = hdr->lqi;
		neighbor_table[neighbor_num].dutycycle = 1;
		neighbor_num++;
	} else {
		neighbor_table[key].rssi = (8*neighbor_table[key].rssi + 2*(-94+3*hdr->rssi))/10; /* when using AT86RF233 transceiver*/
		neighbor_table[key].lqi = (8*neighbor_table[key].lqi + 2*hdr->lqi)/10;
	}
	//printf("neighbor: addr %4x, rssi %d, lqi %u\n", neighbor_table[key].addr, neighbor_table[key].rssi, neighbor_table[key].lqi);
}

static bool is_receiving(netdev_t* dev) {
	netopt_state_t state;
	int rv = dev->driver->get(dev, NETOPT_STATE, &state, sizeof(state));
	if (rv != sizeof(state)) {
		assert(false);
	}
	return state == NETOPT_STATE_RX;
}

uint16_t global_src_l2addr;
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
	else if (event == NETDEV_EVENT_RX_DATAREQ) {
		rx_data_request = true;
	}
    else {
        DEBUG("gnrc_netdev: event triggered -> %i\n", event);
		bool will_retry;
        switch(event) {
            case NETDEV_EVENT_RX_COMPLETE:
                {
                    gnrc_pktsnip_t *pkt = gnrc_dutymac_netdev->recv(gnrc_dutymac_netdev);

					/* Extract src addr and update neighbor table */
					gnrc_pktsnip_t *temp_pkt = pkt;
					while (temp_pkt->next) { temp_pkt = temp_pkt->next; }
					gnrc_netif_hdr_t *hdr = temp_pkt->data;
					uint8_t* src_addr = gnrc_netif_hdr_get_src_addr(hdr);
					uint16_t src_l2addr = 0;
					if (hdr->src_l2addr_len == IEEE802154_SHORT_ADDRESS_LEN) {
						src_l2addr = (((uint16_t) src_addr[1]) << 8) | (uint16_t) src_addr[0];
					} else {
						src_l2addr = (((uint16_t) src_addr[7]) << 8) | (uint16_t) src_addr[6];
					}
					neighbor_table_update(src_l2addr, hdr);

					global_src_l2addr = src_l2addr;

					/* Send packets when receiving a data req from a leaf node */
					if (rx_data_request && pending_num) {
						msg_t msg;
						msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_SND;
						msg.content.ptr = &global_src_l2addr;
						msg_send(&msg, gnrc_dutymac_netdev->pid);
					}
					rx_data_request = false;

				    if (pkt) {
                        _pass_on_packet(pkt);
                    }
                    break;
                }
            case NETDEV_EVENT_TX_COMPLETE:
#ifdef MODULE_NETSTATS_L2
         	    dev->stats.tx_success++;
#endif
				csma_send_succeeded();
				retry_send_succeeded();
				radio_busy = false; /* radio is free now */
				/* Remove only unicasting packets, broadcasting packets are removed by timer expires */
				if (broadcasting) {
					recent_dst_l2addr = 0xffff;
				} else {
					msg_t msg;
					msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
					msg_send(&msg, gnrc_dutymac_netdev->pid);
				}
			    break;
			case NETDEV_EVENT_TX_MEDIUM_BUSY:
#ifdef MODULE_NETSTATS_L2
                dev->stats.tx_failed++;
#endif
				will_retry = csma_send_failed();
				if (will_retry) {
					break;
				}
				/* Fallthrough intentional */
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

				radio_busy = false; /* radio is free now */
				/* Remove only unicasting packets, broadcasting packets are removed by timer expires */
				if (broadcasting) {
					recent_dst_l2addr = 0xffff;
				} else {
					msg_t msg;
					msg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
					msg_send(&msg, gnrc_dutymac_netdev->pid);
				}
				break;
            default:
                printf("gnrc_netdev: warning: unhandled event %u.\n", event);
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

	timer.callback = broadcast_cb;
	timer.arg = (void*) gnrc_dutymac_netdev;

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

	/* setup the link layer neighbor table */
	for (int i=0; i<NEIGHBOR_TABLE_SIZE; i++) {
		neighbor_table[i].addr = 0;
		neighbor_table[i].rssi = 0;
		neighbor_table[i].etx = 0;
		neighbor_table[i].dutycycle = 0xffff;
	}

    /* register the event callback with the device driver */
    dev->event_callback = _event_cb;
    dev->context = (void*) gnrc_dutymac_netdev;

    /* register the device to the network stack*/
    gnrc_netif_add(thread_getpid());

    /* initialize low-level driver (listening mode) */
    dev->driver->init(dev);
	netopt_state_t sleepstate = NETOPT_STATE_IDLE;
    dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));

	{
		bool pending = false;
		dev->driver->set(dev, NETOPT_ACK_PENDING, &pending, sizeof(bool));
	}

    /* start the event loop */
    while (1) {
        DEBUG("gnrc_netdev: waiting for incoming messages\n");
        msg_receive(&msg);

        /* dispatch NETDEV and NETAPI messages */
        switch (msg.type) {
			case GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_SND:
				/* Send a packet in the packet queue if its destination matches to the input address */
				if (pending_num && !radio_busy) {
					msg_queue_send(pkt_queue, true, *((uint16_t*)msg.content.ptr), gnrc_dutymac_netdev);
				}
				break;
			case GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE:
				/* Remove a packet from the packet queue */
				msg_queue_remove(pkt_queue);
				/* Send a packet in the packet queue */
				/* */
				if (pending_num && !radio_busy && recent_dst_l2addr != 0xffff && !irq_pending && !is_receiving(dev)) {
					/* Send a packet to the same destination */
					msg_queue_send(pkt_queue, true, recent_dst_l2addr, gnrc_dutymac_netdev);
					if (!radio_busy && !irq_pending && !is_receiving(dev)) {
						/* If there are no packets with the same destination, check for packets destined for always-on nodes. */
						msg_queue_send(pkt_queue, false, 0, gnrc_dutymac_netdev);
					}
				} else if (!pending_num) {
					bool pending = false;
                	dev->driver->set(dev, NETOPT_ACK_PENDING, &pending, sizeof(bool));
				}
				break;
			case GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_CHECK_QUEUE:
				if (!radio_busy && !irq_pending && !is_receiving(dev)) {
					msg_queue_send(pkt_queue, false, 0, gnrc_dutymac_netdev);
				}
				break;
            case NETDEV_MSG_TYPE_EVENT:
                DEBUG("gnrc_netdev: GNRC_NETDEV_MSG_TYPE_EVENT received\n");
				irq_pending = false;
                dev->driver->isr(dev);
				{
					msg_t nmsg;
					nmsg.type = GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_CHECK_QUEUE;
					nmsg.content.ptr = NULL;
					msg_send_to_self(&nmsg);
				}
                break;
            case GNRC_NETAPI_MSG_TYPE_SND:
                DEBUG("gnrc_netdev: GNRC_NETAPI_MSG_TYPE_SND received\n");
				/* ToDo: We need to distingush sending operation according to the destination
						characteristisc: duty-cycling or always-on */
				/* Queue a packet */
				if (msg_queue_add(pkt_queue, &msg, gnrc_dutymac_netdev)) {
					/* If a packet exists, send ACKs with pending bit */
					bool pending = true;
                	dev->driver->set(dev, NETOPT_ACK_PENDING, &pending, sizeof(bool));

					if (!radio_busy && !irq_pending && !is_receiving(dev)) {
						/* If we added something to the queue, check for packets destined for always-on nodes.
						 * If the radio is busy now, it's OK. We will do this same check whenever the radio
						 * goes from busy to not busy.
						 */
						msg_queue_send(pkt_queue, false, 0, gnrc_dutymac_netdev);
					}
				} else {
					gnrc_pktbuf_release(msg.content.ptr);
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
			case GNRC_NETDEV_DUTYCYCLE_MSG_TYPE_LINK_RETRANSMIT:
				if (!irq_pending && !is_receiving(dev)) {
					if (retry_rexmit) {
						res = gnrc_dutymac_netdev->resend_without_release(gnrc_dutymac_netdev, msg.content.ptr, pending_num > 1);
					} else {
						res = gnrc_dutymac_netdev->send_without_release(gnrc_dutymac_netdev, msg.content.ptr, pending_num > 1);
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
