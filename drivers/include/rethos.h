/*
 * Copyright (C) 2016 Michael Andersen <m.andersen@cs.berkeley.edu>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup    drivers_ethos rethos
 * @ingroup     drivers_netdev
 * @brief       Driver for the Really EveryTHing over-serial module
 * @{
 *
 * @file
 * @brief       Interface definition for the rethos module
 *
 * @author      Michael Andersen <m.andersen@cs.berkeley.edu>
 */

#ifndef RETHOS_H
#define RETHOS_H

#include "kernel_types.h"
#include "periph/uart.h"
#include "net/netdev2.h"
#include "tsrb.h"
#include "mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* rethos is a drop in replacement for ethos, so shares some of its
  symbols. The difference between an ETHOS and RETHOS frame is as follows:

A RETHOS frame looks like:
<escape><frame start><frame type (1 byte)><frame seq 2 bytes><frame channel (1 byte)> .. frame .. <escape><frame end><frame checksum (2 bytes after unescape)>

a RETHOS implementation may drop frames whose types it does not recognize, or
frames on channels that have no registered listener

Note that it is illegal for any character in the preamble to be 0xBE (the escape)
so that implies that even the preamble needs to be escaped when written
*/

/* if using ethos + stdio, use UART_STDIO values unless overridden */
#ifdef USE_ETHOS_FOR_STDIO
#include "uart_stdio.h"
#ifndef ETHOS_UART
#define ETHOS_UART     UART_STDIO_DEV
#endif
#ifndef ETHOS_BAUDRATE
#define ETHOS_BAUDRATE UART_STDIO_BAUDRATE
#endif
#endif

#ifndef RETHOS_TX_BUF_SZ
#define RETHOS_TX_BUF_SZ 2048
#endif

#ifndef RETHOS_RX_BUF_SZ
#define RETHOS_RX_BUF_SZ 2048
#endif

/**
 * @name Escape char definitions
 * @{
 */
#define RETHOS_ESC_CHAR                  (0xBE)
/* This means that a stream of ESC_CHAR still keeps us inside the escape state */
#define RETHOS_LITERAL_ESC               (0x55)
#define RETHOS_FRAME_START               (0xEF)
#define RETHOS_FRAME_END                 (0xE5)

#define RETHOS_FRAME_TYPE_DATA           (0x1)

#define RETHOS_FRAME_TYPE_HB             (0x2)
#define RETHOS_FRAME_TYPE_HB_REPLY       (0x3)

/* Sam: I am going to remove this because I don't use it at all.
#define RETHOS_FRAME_TYPE_SETMAC         (0x4)
*/

#define RETHOS_FRAME_TYPE_ACK            (0x4)
#define RETHOS_FRAME_TYPE_NACK           (0x5)

#define RETHOS_CHANNEL_CONTROL            0x00
#define RETHOS_CHANNEL_NETDEV             0x01
#define RETHOS_CHANNEL_STDIO              0x02


/* Retransmit interval in microseconds. */
#define RETHOS_REXMIT_MICROS 100000L

/** @} */

/**
 * @brief   enum describing line state
 */
typedef enum {
    SM_WAIT_FRAMESTART,
    SM_WAIT_TYPE,
    SM_WAIT_SEQ0,
    SM_WAIT_SEQ1,
    SM_WAIT_CHANNEL,
    SM_IN_FRAME,
    SM_WAIT_CKSUM1,
    SM_WAIT_CKSUM2,
    SM_IN_ESCAPE
} line_state_t;

struct _rethos_handler;

typedef struct _rethos_handler rethos_handler_t;

/**
 * @brief ethos netdev2 device
 * @extends netdev2_t
 */
typedef struct {
    netdev2_t netdev;       /**< extended netdev2 structure */
    uart_t uart;            /**< UART device the to use */
    uint8_t mac_addr[6];    /**< this device's MAC address */
    uint8_t remote_mac_addr[6]; /**< this device's MAC address */

    line_state_t state;     /**< Line status variable */
    line_state_t fromstate; /**< what you go back to after escape */
  //  size_t framesize;       /**< size of currently incoming frame */
  //  unsigned frametype;     /**< type of currently incoming frame */
  //  size_t last_framesize;  /**< size of last completed frame */
    mutex_t out_mutex;      /**< mutex used for locking concurrent sends */

    tsrb_t netdev_inbuf;           /**< ringbuffer for incoming netdev data */
    size_t netdev_packetsz;

    uint8_t rx_buffer [RETHOS_RX_BUF_SZ];
    size_t rx_buffer_index;
    uint8_t rx_frame_type;
    uint16_t rx_seqno;
    uint8_t rx_channel;
    uint16_t rx_cksum1;
    uint16_t rx_cksum2;
    uint16_t rx_actual_cksum; //The data
    uint16_t rx_expected_cksum; //The header

    rethos_handler_t *handlers;

    uint32_t stats_rx_cksum_fail;
    uint32_t stats_rx_bytes;
    uint32_t stats_rx_frames;

    uint32_t stats_tx_bytes;
    uint32_t stats_tx_frames;
    uint32_t stats_tx_retries;

  //  uint8_t txframebuf [RETHOS_TX_BUF];
    uint16_t txseq;
  //  uint16_t txlen;
    uint16_t flsum1;
    uint16_t flsum2;

    /* State for retransmissions. */
    uint16_t rexmit_seqno;
    uint8_t rexmit_channel;
    size_t rexmit_numbytes;
    uint8_t rexmit_frame[RETHOS_TX_BUF_SZ];
    bool rexmit_acked;

    bool received_data;
    uint16_t last_rcvd_seqno;
} ethos_t;

struct _rethos_handler {
  struct _rethos_handler *_next;
  void (*cb)(ethos_t *dev, uint8_t channel, const uint8_t *data, uint16_t length);
  uint8_t channel;
};

/**
 * @brief   Struct containing the needed configuration
 */
typedef struct {
    uart_t uart;            /**< UART device to use */
    uint32_t baudrate;      /**< baudrate to UART device */
    uint8_t *buf;           /**< buffer for incoming packets */
    size_t bufsize;         /**< size of ethos_params_t::buf */
} ethos_params_t;

/**
 * @brief Setup an ethos based device state.
 *
 * The supplied buffer *must* have a power-of-two size, and it *must* be large
 * enough for the largest expected packet + enough buffer space to buffer
 * bytes that arrive while one packet is being handled.
 * note that rethos needs a bigger buffer than ethos because it buffers up to TWO frames
 *
 * E.g., if 1536b ethernet frames are expected, 4096 is probably a good size for @p buf.
 *
 * @param[out]  dev         handle of the device to initialize
 * @param[in]   params      parameters for device initialization
 */
void ethos_setup(ethos_t *dev, const ethos_params_t *params);

void rethos_rexmit_callback(void* arg);

/**
 * @brief send frame over serial port using ethos' framing
 *
 * This is used by e.g., stdio over ethos to send text frames.
 *
 * @param[in]   dev         handle of the device to initialize
 * @param[in]   data        ptr to data to be sent
 * @param[in]   len         nr of bytes to send
 * @param[in]   frame_type  frame channel to use
 */
void ethos_send_frame(ethos_t *dev, const uint8_t *data, size_t len, unsigned channel);

/**
 * @brief send frame over serial port using ethos' framing
 *
 * This is used by e.g., stdio over ethos to send text frames.
 *
 * @param[in]   dev         handle of the device to initialize
 * @param[in]   data        ptr to data to be sent
 * @param[in]   len         nr of bytes to send
 * @param[in]   frame_type  frame channel to use
 */
void rethos_send_frame(ethos_t *dev, const uint8_t *data, size_t len, uint8_t channel, uint8_t frame_type);

void rethos_rexmit_data_frame(ethos_t* dev);

void rethos_send_ack_frame(ethos_t* dev, uint16_t seqno);

void rethos_send_nack_frame(ethos_t* dev);

/**
 * @brief send frame over serial port using ethos' framing
 *
 * This is used by e.g., stdio over ethos to send text frames.
 *
 * @param[in]   dev         handle of the device to initialize
 * @param[in]   data        ptr to data to be sent
 * @param[in]   thislen     nr of bytes to send on this invocation
 * @param[in]   frame_type  frame type to use
 */
void rethos_start_frame(ethos_t *dev, const uint8_t *data, size_t thislen, uint8_t channel, uint8_t frame_type);

/**
 * @brief send frame over serial port using ethos' framing
 *
 * This is used by e.g., stdio over ethos to send text frames.
 *
 * @param[in]   dev         handle of the device to initialize
 * @param[in]   data        ptr to data to be sent
 * @param[in]   thislen     nr of bytes to send on this invocation
 */
void rethos_continue_frame(ethos_t *dev, const uint8_t *data, size_t thislen);

/**
 * @brief send frame over serial port using ethos' framing
 *
 * This is used by e.g., stdio over ethos to send text frames.
 *
 * @param[in]   dev         handle of the device to initialize
 * @param[in]   data        ptr to data to be sent
 * @param[in]   thislen     nr of bytes to send on this invocation
 * @param[in]   frame_type  frame type to use
 */
void rethos_end_frame(ethos_t *dev);



void rethos_register_handler(ethos_t *dev, rethos_handler_t *handler);

#ifdef __cplusplus
}
#endif
#endif /* RETHOS_H */
/** @} */
