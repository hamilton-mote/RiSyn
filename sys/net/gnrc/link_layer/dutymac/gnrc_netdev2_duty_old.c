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
 * @brief       Glue for netdev2 devices to netapi
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
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


#include "xtimer.h"

#if DUTYCYCLE_EN
#if EEEEEEEEEE

#define ENABLE_DEBUG    (0)
#include "debug.h"

#if defined(MODULE_OD) && ENABLE_DEBUG
#include "od.h"
#endif

#define NETDEV2_NETAPI_MSG_QUEUE_SIZE 8
#define NETDEV2_PKT_QUEUE_SIZE 4

#if LEAF_NODE
#else
#define NEIGHBOR_TABLE_SIZE 10
link_neighbor_table_t neighbor_table[NEIGHBOR_TABLE_SIZE];
uint8_t neighbor_num = 0;
#endif

static void _pass_on_packet(gnrc_pktsnip_t *pkt);

/** 1) For a leaf node (battery-powered), 'dutycycling' is set to NETOPT_ENABLE by application 
 *  2) For a router (wall-powered), 'dutycycling' remains to NETOPT_DISABLE 
 */
netopt_enable_t   dutycycling     = NETOPT_DISABLE;

/*  Dutycycle state (INIT, SLEEP, TXBEACON, TXDATA, and LISTEN) */
dutycycle_state_t dutycycle_state = DUTY_INIT;

/**	1) For a leaf node, 'timer' is used for wake-up scheduling
 *  2) For a router, 'timer' is used for broadcasting; 
 *     a router does not discard a broadcasting packet during a sleep interval
 */
xtimer_t timer;
uint8_t broadcasting = 0;
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
uint8_t radio_busy = 0;

/* This is the packet being sent by the radio now */
gnrc_pktsnip_t *current_pkt;


// FIFO version
//int msg_queue_add(msg_t* msg_queue, msg_t* msg) {
//	if (pending_num < NETDEV2_PKT_QUEUE_SIZE) {
		/* Add a packet to the last entry of the queue */
//		msg_queue[pending_num].sender_pid = msg->sender_pid;
//		msg_queue[pending_num].type = msg->type;
//		msg_queue[pending_num].content.ptr = msg->content.ptr;
//		printf("\nqueue add success [%u/%u/%4x]\n", pending_num, msg_queue[pending_num].sender_pid, 
//				msg_queue[pending_num].type);

		/** When it is the first and broadcasting packet and the nodes is a router,
		  * MAC maintains the packet for a sleep interval to send it to all neighbors 
		  */ 
//		if (pending_num == 0 && dutycycling == NETOPT_DISABLE) {
//			gnrc_pktsnip_t *pkt = msg_queue[pending_num].content.ptr;
//			gnrc_netif_hdr_t* hdr = pkt->data;
//			if (hdr->flags & (GNRC_NETIF_HDR_FLAGS_BROADCAST | GNRC_NETIF_HDR_FLAGS_MULTICAST)) {
//				xtimer_set(&timer, DUTYCYCLE_SLEEP_INTERVAL+100);
//				broadcasting = 1;
//				printf("broadcast starts\n");
//			}
//		}
//		pending_num++; /* Number of packets in the queue */			
//		return 1;
//	} else {
//		DEBUG("Queue loss at netdev2\n");
//		return 0;
//	}
//}

// Exhaustive search version
int msg_queue_add(msg_t* msg_queue, msg_t* msg) {
	if (pending_num < NETDEV2_PKT_QUEUE_SIZE) {
		gnrc_pktsnip_t *pkt = msg->content.ptr;
		gnrc_netif_hdr_t* hdr = pkt->data;
		
		// 1) Broadcasting packet (Insert head of the queue)
		if (dutycycling == NETOPT_DISABLE && 
			(hdr->flags & (GNRC_NETIF_HDR_FLAGS_BROADCAST | GNRC_NETIF_HDR_FLAGS_MULTICAST))) {
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
				broadcasting = 1;
				sending_pkt_key = 0;
				printf("broadcast starts\n");			
			} 
			broadcasting_num++;
		} 
		// 2) Unicasting packet
		else {
			/* Add a packet to the last entry of the queue */
			msg_queue[pending_num].sender_pid = msg->sender_pid;
			msg_queue[pending_num].type = msg->type;
			msg_queue[pending_num].content.ptr = msg->content.ptr;
			printf("\nqueue add success [%u/%u/%4x]\n", pending_num, msg_queue[pending_num].sender_pid, 
					msg_queue[pending_num].type);
		}			
		pending_num++; /* Number of packets in the queue */			
		return 1;
	} else {
		DEBUG("Queue loss at netdev2\n");
		return 0;
	}
}

void msg_queue_remove_head(msg_t* msg_queue) {
	/* Remove queue head */	
	if (sending_pkt_key == 0xFF) 
		return;

	printf("remove queue [%u, %u/%u]\n", sending_pkt_key, broadcasting_num, pending_num--);

	gnrc_pktbuf_release(msg_queue[sending_pkt_key].content.ptr);
	pending_num--;
	if (pending_num < 0) {
		DEBUG("NETDEV2: Pending number error\n");
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
		if (dutycycling == NETOPT_DISABLE && broadcasting_num > 0) {
			xtimer_set(&timer, DUTYCYCLE_SLEEP_INTERVAL+100);
			broadcasting = 1;
			sending_pkt_key = 0;
			printf("broadcast starts\n");
			return;
		}
	}
	sending_pkt_key = 0xFF;
	return;
}

//void msg_queue_send(msg_t* msg_queue, uint16_t dst_l2addr, gnrc_netdev2_t* gnrc_dutymac_netdev2) {
//	gnrc_pktsnip_t *pkt = msg_queue[0].content.ptr;
//	gnrc_netif_hdr_t* hdr = pkt->data;
//	uint8_t* dst  = gnrc_netif_hdr_get_dst_addr(hdr); 
//	uint16_t pkt_dst_l2addr;
//	if (hdr->dst_l2addr_len == IEEE802154_SHORT_ADDRESS_LEN) {
//		pkt_dst_l2addr = (*dst | (*(dst+1) << 8));
//	} else {
//		pkt_dst_l2addr = (*dst<<8 | (*(dst+1)));
//	}
	//printf("sendid %u, type %4x, dst: %4x\n", msg_queue[0].sender_pid, msg_queue[0].type, pkt_dst_l2addr);

//	if (pkt_dst_l2addr == dst_l2addr) {
//		(hdr->flags & (GNRC_NETIF_HDR_FLAGS_BROADCAST | GNRC_NETIF_HDR_FLAGS_MULTICAST))) {
//		recent_dst_l2addr = pkt_dst_l2addr;
//		gnrc_pktsnip_t* temp_pkt = pkt;
//		gnrc_pktsnip_t *p1, *p2;
//		current_pkt = gnrc_pktbuf_add(NULL, temp_pkt->data, temp_pkt->size, temp_pkt->type);
//		p1 = current_pkt;
//		temp_pkt = temp_pkt->next;
//		while(temp_pkt) {
//			p2 = gnrc_pktbuf_add(NULL, temp_pkt->data, temp_pkt->size, temp_pkt->type);
//			p1->next = p2;
//			p1 = p1->next;
//			temp_pkt = temp_pkt->next;
//		}
//		radio_busy = 1; /* radio is now busy */
//		gnrc_dutymac_netdev2->send(gnrc_dutymac_netdev2, current_pkt);	
//	}
//}

void msg_queue_send(msg_t* msg_queue, uint16_t dst_l2addr, gnrc_netdev2_t* gnrc_dutymac_netdev2) {
	gnrc_pktsnip_t *pkt = NULL; 

	if (broadcasting) { // broadcasting
		pkt = msg_queue[0].content.ptr;
		sending_pkt_key = 0;
		recent_dst_l2addr = 0xFFFF;

	} else {            // unicasting
		gnrc_pktsnip_t *temp_pkt;
		gnrc_netif_hdr_t *temp_hdr;
		uint16_t pkt_dst_l2addr;
		uint8_t* dst;
		for (int i=0; i<pending_num; i++) {
			temp_pkt = msg_queue[i].content.ptr;
			temp_hdr = temp_pkt->data;
			dst = gnrc_netif_hdr_get_dst_addr(temp_hdr); 
			if (temp_hdr->dst_l2addr_len == IEEE802154_SHORT_ADDRESS_LEN) {
				pkt_dst_l2addr = (*dst | (*(dst+1) << 8));
			} else {
				pkt_dst_l2addr = (*dst<<8 | (*(dst+1)));
			}
		
			if (pkt_dst_l2addr == dst_l2addr) {	
				pkt = msg_queue[i].content.ptr;
				recent_dst_l2addr = pkt_dst_l2addr;
				sending_pkt_key = i;
				break;	
			}
		}
	}

	if (pkt != NULL && sending_pkt_key != 0xFF) {
		//printf("sending %u to %4x (%u/%u)\n", sending_pkt_key, recent_dst_l2addr, broadcasting_num, pending_num);
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
}

/**
 * @brief   Function called by the dutycycle timer
 *
 * @param[in] event     type of event
 */
void dutycycle_cb(void* arg) {
	gnrc_netdev2_t* gnrc_dutymac_netdev2 = (gnrc_netdev2_t*) arg;
    msg_t msg;
	if (dutycycling == NETOPT_ENABLE) {
		/* Dutycycling state control for leaf nodes */
    	msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_EVENT;
		switch(dutycycle_state) {
			case DUTY_INIT:
				break;
			case DUTY_SLEEP:
				if (pending_num) {
					dutycycle_state = DUTY_TX_DATA;
				} else { 
					dutycycle_state = DUTY_TX_BEACON;
				}
				break;
			case DUTY_TX_BEACON:
				dutycycle_state = DUTY_LISTEN;
				break;
			case DUTY_TX_DATA:
				dutycycle_state = DUTY_LISTEN;
				break;
			case DUTY_LISTEN:
				dutycycle_state = DUTY_SLEEP;
				break;
			default:
				break;
		}
	} else {
		/* Broadcasting msg maintenance for routers */
		broadcasting = 0;
		broadcasting_num--;
		printf("braodcast ends\n");
		msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
	}
	msg_send(&msg, gnrc_dutymac_netdev2->pid);
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
		neighbor_table[neighbor_num].addr = l2addr;
		neighbor_table[neighbor_num].rssi = hdr->rssi;
		neighbor_table[neighbor_num].lqi  = hdr->lqi;
		neighbor_table[neighbor_num].dutycycle = 1;
		neighbor_num++;
	} else {
		neighbor_table[key].rssi = (8*neighbor_table[key].rssi + 2*hdr->rssi)/10;
		neighbor_table[key].lqi = (8*neighbor_table[key].lqi + 2*hdr->lqi)/10;
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
    else {
        DEBUG("gnrc_netdev2: event triggered -> %i\n", event);
        switch(event) {
            case NETDEV2_EVENT_RX_COMPLETE:
                {
                    gnrc_pktsnip_t *pkt = gnrc_dutymac_netdev2->recv(gnrc_dutymac_netdev2);
					if (dutycycling == NETOPT_ENABLE) {
						xtimer_remove(&timer);
						dutycycle_cb((void*)gnrc_dutymac_netdev2);
					} else {
						gnrc_pktsnip_t *temp_pkt = pkt;
						while (temp_pkt->next) { temp_pkt = temp_pkt->next; }
						gnrc_netif_hdr_t *hdr = temp_pkt->data;
						uint8_t* src_addr = gnrc_netif_hdr_get_src_addr(hdr);
						uint16_t src_l2addr = 0;
						if (hdr->src_l2addr_len == IEEE802154_SHORT_ADDRESS_LEN) {
							src_l2addr = (*src_addr | (*(src_addr+1) << 8));							
						} else {
							src_l2addr = (*src_addr << 8| (*(src_addr+1)));
						}
						msg_t msg;
						msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_SND;
						msg.content.ptr = &src_l2addr;
						msg_send(&msg, gnrc_dutymac_netdev2->pid);
						neighbor_table_update(src_l2addr, hdr);
					}
                    if (pkt) {
                        _pass_on_packet(pkt);
                    }					
                    break;
                }
            case NETDEV2_EVENT_TX_MEDIUM_BUSY:
#ifdef MODULE_NETSTATS_L2
                dev->stats.tx_failed++;
#endif
                break;
            case NETDEV2_EVENT_TX_COMPLETE:
#ifdef MODULE_NETSTATS_L2
         	    dev->stats.tx_success++;
#endif
				radio_busy = 0; /* radio is free now */				
				if (dutycycling == NETOPT_ENABLE) {
					xtimer_remove(&timer);
					if (dutycycle_state == DUTY_TX_BEACON) {
						dutycycle_cb((void*)gnrc_dutymac_netdev2);
					} else {
						msg_t msg;
						msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
						msg_send(&msg, gnrc_dutymac_netdev2->pid);
					}
				} else {
					/* Remove only unicasting packets, broadcasting packets are removed by timer expires */
					if (!broadcasting) { 
						msg_t msg;
						msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
						msg_send(&msg, gnrc_dutymac_netdev2->pid);
					}
				}
	            break;
			case NETDEV2_EVENT_TX_NOACK:
				radio_busy = 0; /* radio is free now */				
				if (dutycycling == NETOPT_ENABLE) {
					xtimer_remove(&timer);
					if (dutycycle_state == DUTY_TX_BEACON) {
						dutycycle_cb((void*)gnrc_dutymac_netdev2);
					} else {
						msg_t msg;
						msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
						msg_send(&msg, gnrc_dutymac_netdev2->pid);
					}
				} else {
					/* Remove only unicasting packets, broadcasting packets are removed by timer expires */
					if (!broadcasting) { 
						msg_t msg;
						msg.type = GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE;
						msg_send(&msg, gnrc_dutymac_netdev2->pid);
					}
				}
				break;
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

#if LEAF_NODE
#else
	/* setup the link layer neighbor table */
	for (int i=0; i<NEIGHBOR_TABLE_SIZE; i++) {
		neighbor_table[i].addr = 0;
		neighbor_table[i].rssi = 0;
		neighbor_table[i].etx = 0;
		neighbor_table[i].dutycycle = 0xffff;
	}
#endif

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
							xtimer_set(&timer,random_uint32_range(0, DUTYCYCLE_SLEEP_INTERVAL)); 		
							break;				
						case DUTY_TX_BEACON: /* Tx a beacon after wake-up */
							gnrc_dutymac_netdev2->send_beacon(gnrc_dutymac_netdev2);
							break;
						case DUTY_TX_DATA:  /* Tx a data packet */
							msg_queue_send(pkt_queue, 0, gnrc_dutymac_netdev2);
							break;
						case DUTY_LISTEN: /* Idle listening after transmission or reception */
							sleepstate = NETOPT_STATE_IDLE;
							dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
							xtimer_set(&timer, DUTYCYCLE_WAKEUP_INTERVAL);
							printf("radio remains on\n");
							break;
						case DUTY_SLEEP: /* Go to sleep */
							sleepstate = NETOPT_STATE_SLEEP;
							xtimer_remove(&timer);
							dev->driver->set(dev, NETOPT_STATE, &sleepstate, sizeof(netopt_state_t));
							printf("radio off\n\n"); 
							xtimer_set(&timer, DUTYCYCLE_SLEEP_INTERVAL);
							break;
						default:
							break;
					}
				} else {
					/* somthing is wrong */
					DEBUG("gnrc_netdev2: SOMETHING IS WRONG\n");
				}
				break;
			case GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_SND:	
				/* Send a packet in the packet queue if its destination matches to the input address */
				if (pending_num && !radio_busy)
					msg_queue_send(pkt_queue, *((uint16_t*)msg.content.ptr), gnrc_dutymac_netdev2);
				break; 
			case GNRC_NETDEV2_DUTYCYCLE_MSG_TYPE_REMOVE_QUEUE:	
				/* Remove a packet from the packet queue */
				msg_queue_remove_head(pkt_queue);
				/* Send a packet in the packet queue */
				if (pending_num && !radio_busy) {
					if (dutycycling == NETOPT_ENABLE) {
						/* Send any packet */
						msg_queue_send(pkt_queue, 0, gnrc_dutymac_netdev2); 
						dutycycle_cb(gnrc_dutymac_netdev2);
					} else {
						/* Send a packet to the same destination */
						msg_queue_send(pkt_queue, recent_dst_l2addr, gnrc_dutymac_netdev2); 
					}
				}
				break;
            case NETDEV2_MSG_TYPE_EVENT:
                DEBUG("gnrc_netdev2: GNRC_NETDEV_MSG_TYPE_EVENT received\n");
                dev->driver->isr(dev);
                break;
            case GNRC_NETAPI_MSG_TYPE_SND:
                DEBUG("gnrc_netdev2: GNRC_NETAPI_MSG_TYPE_SND received\n");
				/* Queue a packet */
				msg_queue_add(pkt_queue, &msg);	
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

					} else {
						/* Dutycycle end triggered by application layer */
						dutycycle_state = DUTY_INIT;
						sleepstate = NETOPT_STATE_IDLE;
					}	
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
#endif
