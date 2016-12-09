/*
 * Copyright (C) 2016 Michael Andersen <m.andersen@berkeley.edu>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     driver_ethos
 * @{
 *
 * @file
 * @brief       A re-implementation of ethos (originally by Kaspar Schleiser)
 *              that creates a reliable multi-channel duplex link over serial
 *
 * @author      Michael Andersen <m.andersen@berkeley.edu>
 *
 * @}
 */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "random.h"
#include "rethos.h"
#include "periph/uart.h"
#include "tsrb.h"
#include "irq.h"

#include "net/netdev2.h"
#include "net/netdev2/eth.h"
#include "net/eui64.h"
#include "net/ethernet.h"

#include <xtimer.h>

#ifdef USE_ETHOS_FOR_STDIO
#include "uart_stdio.h"
#endif

#define ENABLE_DEBUG (0)
#include "debug.h"

static void _get_mac_addr(netdev2_t *dev, uint8_t* buf);
static void ethos_isr(void *arg, uint8_t c);
static const netdev2_driver_t netdev2_driver_ethos;

static const uint8_t _esc_esc[] = {RETHOS_ESC_CHAR, RETHOS_LITERAL_ESC};
static const uint8_t _start_frame[] = {RETHOS_ESC_CHAR, RETHOS_FRAME_START};
static const uint8_t _end_frame[] = {RETHOS_ESC_CHAR, RETHOS_FRAME_END};

xtimer_t rexmit_timer;

void rethos_send_frame_seqno_norexmit(ethos_t *dev, const uint8_t *data, size_t len, uint8_t channel, uint16_t seqno, uint8_t frame_type);
void rethos_start_frame_seqno_norexmit(ethos_t* dev, const uint8_t* data, size_t thislen, uint8_t channel, uint16_t seqno, uint8_t frame_type);
void rethos_end_frame_helper(ethos_t* dev, bool rexmit);

static void fletcher16_add(const uint8_t *data, size_t bytes, uint16_t *sum1i, uint16_t *sum2i)
{
    uint16_t sum1 = *sum1i, sum2 = *sum2i;

    while (bytes) {
        size_t tlen = bytes > 20 ? 20 : bytes;
        bytes -= tlen;
        do {
            sum2 += sum1 += *data++;
        } while (--tlen);
        sum1 = (sum1 & 0xff) + (sum1 >> 8);
        sum2 = (sum2 & 0xff) + (sum2 >> 8);
    }
    *sum1i = sum1;
    *sum2i = sum2;
}

static uint16_t fletcher16_fin(uint16_t sum1, uint16_t sum2)
{
    sum1 = (sum1 & 0xff) + (sum1 >> 8);
    sum2 = (sum2 & 0xff) + (sum2 >> 8);
    return (sum2 << 8) | sum1;
}

void ethos_setup(ethos_t *dev, const ethos_params_t *params)
{
    dev->netdev.driver = &netdev2_driver_ethos;
    dev->uart = params->uart;
    dev->state = SM_WAIT_FRAMESTART;
    dev->netdev_packetsz = 0;
    dev->rx_buffer_index = 0;
    dev->handlers = NULL;
    dev->txseq = 0;
    dev->stats_tx_frames = 0;
    dev->stats_tx_retries = 0;
    dev->stats_tx_bytes = 0;
    dev->stats_rx_frames = 0;
    dev->stats_rx_cksum_fail = 0;
    dev->stats_rx_bytes = 0;

    dev->rexmit_acked = true;
    dev->received_data = false;

    tsrb_init(&dev->netdev_inbuf, (char*)params->buf, params->bufsize);
    mutex_init(&dev->out_mutex);

    uint32_t a = random_uint32();
    memcpy(dev->mac_addr, (char*)&a, 4);
    a = random_uint32();
    memcpy(dev->mac_addr+4, (char*)&a, 2);

    dev->mac_addr[0] &= (0x2);      /* unset globally unique bit */
    dev->mac_addr[0] &= ~(0x1);     /* set unicast bit*/

    rexmit_timer.callback = rethos_rexmit_callback;

    uart_init(params->uart, params->baudrate, ethos_isr, (void*)dev);

    //TODO send mac address
    //
    // uint8_t frame_delim = ETHOS_FRAME_DELIMITER;
    // uart_write(dev->uart, &frame_delim, 1);
    // ethos_send_frame(dev, dev->mac_addr, 6, ETHOS_FRAME_TYPE_HELLO);
}

static void sm_invalidate(ethos_t *dev)
{
    dev->state = SM_WAIT_FRAMESTART;
    dev->rx_buffer_index = 0;
}
static void process_frame(ethos_t *dev)
{
    /* Sam: Michael, I have no idea what you're doing here.
    if (dev->rx_frame_type == RETHOS_FRAME_TYPE_SETMAC)
    {
      memcpy(&dev->remote_mac_addr, dev->rx_buffer, 6);
      rethos_send_frame(dev, dev->mac_addr, 6, RETHOS_CHANNEL_CONTROL, RETHOS_FRAME_TYPE_SETMAC);
    }
    */
    if (dev->rx_frame_type != RETHOS_FRAME_TYPE_DATA)
    {
        /* All ACKs and NACKs happen on the RETHOS-reserved channel. */
        if (dev->rx_channel == RETHOS_CHANNEL_CONTROL)
        {
            if (dev->rx_frame_type == RETHOS_FRAME_TYPE_ACK)
            {
                if (dev->rx_seqno == dev->rexmit_seqno)
                {
                    dev->rexmit_acked = true;
                    xtimer_remove(&rexmit_timer);
                }
            }
            else if (dev->rx_frame_type == RETHOS_FRAME_TYPE_NACK)
            {
                if (dev->rexmit_acked)
                {
                    /* They've already ACKed the last thing we sent, so either one of NACKs got
                     * corrupted or one of our ACKs got corrupted.
                     *
                     * Sending a NACK here could cause a NACK storm, so instead just ACK the last
                     * thing we received.
                     */
                    if (dev->received_data)
                    {
                        rethos_send_ack_frame(dev, dev->rx_seqno);
                    }
                }
                else
                {
                    /* Retransmit the last data frame we sent. */
                    rethos_rexmit_data_frame(dev);
                }
            }
        }

        return; //Other types are internal to rethos
    }

    dev->received_data = true;
    dev->last_rcvd_seqno = dev->rx_seqno;

    /* ACK the frame we just received. */
    rethos_send_ack_frame(dev, dev->rx_seqno);

    //Handle the special channels
    switch(dev->rx_channel) {
      case RETHOS_CHANNEL_NETDEV:
        tsrb_add(&dev->netdev_inbuf, (char*) dev->rx_buffer, dev->rx_buffer_index);
        dev->netdev_packetsz = dev->rx_buffer_index;
        dev->netdev.event_callback((netdev2_t*) dev, NETDEV2_EVENT_ISR);
        break;
      case RETHOS_CHANNEL_STDIO:
        for (size_t i = 0; i < dev->rx_buffer_index; i++)
        {
          uart_stdio_rx_cb(NULL, dev->rx_buffer[i]);
        }
        break;
      default:
        break;
    }
    //And all registered handlers
    rethos_handler_t *h = dev->handlers;
    while (h != NULL)
    {
      if (h->channel == dev->rx_channel) {
        h->cb(dev, dev->rx_channel, dev->rx_buffer, dev->rx_buffer_index);
      }
      h = h->_next;
    }
}

static void sm_char(ethos_t *dev, uint8_t c)
{
  switch (dev->state)
  {
    case SM_WAIT_TYPE:
      dev->rx_frame_type = c;
      fletcher16_add(&c, 1, &dev->rx_cksum1, &dev->rx_cksum2);
      dev->state = SM_WAIT_SEQ0;
      return;
    case SM_WAIT_SEQ0:
      dev->rx_seqno = c;
      fletcher16_add(&c, 1, &dev->rx_cksum1, &dev->rx_cksum2);
      dev->state = SM_WAIT_SEQ1;
      return;
    case SM_WAIT_SEQ1:
      dev->rx_seqno |= (((uint16_t)c)<<8);
      fletcher16_add(&c, 1, &dev->rx_cksum1, &dev->rx_cksum2);
      dev->state = SM_WAIT_CHANNEL;
      return;
    case SM_WAIT_CHANNEL:
      dev->rx_channel = c;
      fletcher16_add(&c, 1, &dev->rx_cksum1, &dev->rx_cksum2);
      dev->state = SM_IN_FRAME;
      return;
    case SM_IN_FRAME:
      dev->rx_buffer[dev->rx_buffer_index] = c;
      fletcher16_add(&c, 1, &dev->rx_cksum1, &dev->rx_cksum2);
      if ((++dev->rx_buffer_index) >= RETHOS_RX_BUF_SZ) {
        sm_invalidate(dev);
      }
      return;
    case SM_WAIT_CKSUM1:
      dev->rx_expected_cksum = c;
      dev->state = SM_WAIT_CKSUM2;
      return;
    case SM_WAIT_CKSUM2:
      dev->rx_expected_cksum |= (((uint16_t)c)<<8);
      if (dev->rx_expected_cksum != dev->rx_actual_cksum)
      {
        dev->stats_rx_cksum_fail++;
        //SAM: do nack or something
        rethos_send_nack_frame(dev);
      } else {
        dev->stats_rx_frames++;
        dev->stats_rx_bytes += dev->rx_buffer_index;
        process_frame(dev);
      }
      sm_invalidate(dev);
      return;
    default:
      return;
  }
}
static void sm_frame_start(ethos_t *dev)
{
  //Drop everything, we are beginning a new frame reception
  dev->state = SM_WAIT_TYPE;
  dev->rx_buffer_index = 0;
  dev->rx_cksum1 = 0xFF;
  dev->rx_cksum2 = 0xFF;
}
//This is not quite the real end of the frame, we still expect the checksum
static void sm_frame_end(ethos_t *dev)
{
  uint16_t cksum = fletcher16_fin(dev->rx_cksum1, dev->rx_cksum2);
  dev->rx_actual_cksum = cksum;
  dev->state = SM_WAIT_CKSUM1;
}
static void ethos_isr(void *arg, uint8_t c)
{
    ethos_t *dev = (ethos_t *) arg;

    if (dev->state == SM_IN_ESCAPE) {
      switch (c) {
        case RETHOS_LITERAL_ESC:
          dev->state = dev->fromstate;
          sm_char(dev, RETHOS_ESC_CHAR);
          return;
        case RETHOS_FRAME_START:
          sm_frame_start(dev);
          return;
        case RETHOS_FRAME_END:
          sm_frame_end(dev);
          return;
        default:
          //any other character is invalid
          sm_invalidate(dev);
          return;
      }
    } else {
      switch(c) {
        case RETHOS_ESC_CHAR:
          dev->fromstate = dev->state;
          dev->state = SM_IN_ESCAPE;
          return;
        default:
          sm_char(dev, c);
          return;
      }
    }
}

//This gets called by netdev2
static void _isr(netdev2_t *netdev)
{
    ethos_t *dev = (ethos_t *) netdev;
    dev->netdev.event_callback((netdev2_t*) dev, NETDEV2_EVENT_RX_COMPLETE);
}

static int _init(netdev2_t *encdev)
{
    ethos_t *dev = (ethos_t *) encdev;
    (void)dev;
    return 0;
}

static size_t iovec_count_total(const struct iovec *vector, int count)
{
    size_t result = 0;
    while(count--) {
        result += vector->iov_len;
        vector++;
    }
    return result;
}

static void _write_escaped(uart_t uart, uint8_t c)
{
    uint8_t *out;
    int n;

    switch(c) {
        case RETHOS_ESC_CHAR:
            out = (uint8_t*)_esc_esc;
            n = 2;
            break;
        default:
            out = &c;
            n = 1;
    }

    uart_write(uart, out, n);
}

void rethos_rexmit_callback(void* arg)
{
    ethos_t* dev = (ethos_t*) arg;
    rethos_rexmit_data_frame(dev);

    xtimer_set(&rexmit_timer, (uint32_t) RETHOS_REXMIT_MICROS);
}

void _start_frame_seqno(ethos_t* dev, const uint8_t* data, size_t thislen, uint8_t channel, uint16_t seqno, uint8_t frame_type)
{
    uint8_t preamble_buffer[6];

    dev->flsum1 = 0xFF;
    dev->flsum2 = 0xFF;

    preamble_buffer[0] = RETHOS_ESC_CHAR;
    preamble_buffer[1] = RETHOS_FRAME_START;
    //This is where the checksum starts
    preamble_buffer[2] = frame_type;
    preamble_buffer[3] = seqno & 0xFF; //Little endian cos im a rebel
    preamble_buffer[4] = seqno >> 8;
    preamble_buffer[5] = channel;

    dev->stats_tx_bytes += 4 + thislen;

    fletcher16_add(&preamble_buffer[2], 4, &dev->flsum1, &dev->flsum2);

    uart_write(dev->uart, preamble_buffer, 2);
    for (size_t i = 0; i < 4; i++)
    {
      _write_escaped(dev->uart, preamble_buffer[2+i]);
    }

    if (thislen > 0)
    {
      fletcher16_add(data, thislen, &dev->flsum1, &dev->flsum2);
      //todo replace with a little bit of chunking
      for (size_t i = 0; i<thislen; i++) {
        _write_escaped(dev->uart, data[i]);
      }
    }
}

void rethos_start_frame_seqno_norexmit(ethos_t* dev, const uint8_t* data, size_t thislen, uint8_t channel, uint16_t seqno, uint8_t frame_type)
{
    if (!irq_is_in()) {
        mutex_lock(&dev->out_mutex);
    }

    _start_frame_seqno(dev, data, thislen, channel, seqno, frame_type);
}

void rethos_start_frame_seqno(ethos_t* dev, const uint8_t* data, size_t thislen, uint8_t channel, uint16_t seqno, uint8_t frame_type)
{
    if (!irq_is_in()) {
        mutex_lock(&dev->out_mutex);
    }

    /* Store this data, in case we need to retransmit it. */
    dev->rexmit_seqno = seqno;
    dev->rexmit_channel = (uint8_t) channel;
    dev->rexmit_numbytes = 0;
    if (dev->rexmit_numbytes + thislen <= RETHOS_TX_BUF_SZ) {
        memcpy(dev->rexmit_frame, data, thislen);
    } else {
        dev->rexmit_numbytes = RETHOS_TX_BUF_SZ + 1;
    }
    dev->rexmit_numbytes += thislen;
    dev->rexmit_acked = true; // We have a partial frame, so don't retransmit it on a NACK

    _start_frame_seqno(dev, data, thislen, channel, seqno, frame_type);
}

void ethos_send_frame(ethos_t *dev, const uint8_t *data, size_t len, unsigned channel)
{
    rethos_send_frame(dev, data, len, channel, RETHOS_FRAME_TYPE_DATA);
}

void rethos_send_frame(ethos_t *dev, const uint8_t *data, size_t len, uint8_t channel, uint8_t frame_type)
{
    rethos_start_frame(dev, data, len, channel, frame_type);
    rethos_end_frame(dev);
}

/* We need to copy this because, apparently, both rethos_send_frame and rethos_start_frame are public... */
void rethos_send_frame_seqno(ethos_t *dev, const uint8_t *data, size_t len, uint8_t channel, uint16_t seqno, uint8_t frame_type)
{
    rethos_start_frame_seqno(dev, data, len, channel, seqno, frame_type);
    rethos_end_frame(dev);
}

void rethos_send_frame_seqno_norexmit(ethos_t *dev, const uint8_t *data, size_t len, uint8_t channel, uint16_t seqno, uint8_t frame_type)
{
    rethos_start_frame_seqno_norexmit(dev, data, len, channel, seqno, frame_type);
    rethos_end_frame_helper(dev, false);
}

void rethos_rexmit_data_frame(ethos_t* dev)
{
    rethos_send_frame_seqno(dev, dev->rexmit_frame, dev->rexmit_numbytes, dev->rexmit_channel, dev->rexmit_seqno, RETHOS_FRAME_TYPE_DATA);
}

void rethos_send_ack_frame(ethos_t* dev, uint16_t seqno)
{
    rethos_send_frame_seqno_norexmit(dev, NULL, 0, RETHOS_CHANNEL_CONTROL, seqno, RETHOS_FRAME_TYPE_ACK);
}

void rethos_send_nack_frame(ethos_t* dev)
{
    rethos_send_frame_seqno_norexmit(dev, NULL, 0, RETHOS_CHANNEL_CONTROL, 0, RETHOS_FRAME_TYPE_NACK);
}

void rethos_start_frame(ethos_t *dev, const uint8_t *data, size_t thislen, uint8_t channel, uint8_t frame_type)
{
    uint16_t seqno = ++(dev->txseq);
    rethos_start_frame_seqno(dev, data, thislen, channel, seqno, frame_type);
}

void rethos_continue_frame(ethos_t *dev, const uint8_t *data, size_t thislen)
{
  fletcher16_add(data, thislen, &dev->flsum1, &dev->flsum2);

  /* Check if we're going to overflow the rexmit buffer. */
  if (thislen + dev->rexmit_numbytes > RETHOS_TX_BUF_SZ)
  {
      /* Just stop transmitting data. The checksum should be corrupt anyway, so
       * the other side won't think this was valid. We just need to make sure we
       * never retransmit.
       */
      dev->rexmit_numbytes = RETHOS_TX_BUF_SZ + 1;
      return;
  }

  /* Store this data, in case we need to retransmit it. */
  if (dev->rexmit_numbytes + thislen <= RETHOS_TX_BUF_SZ) {
      memcpy(dev->rexmit_frame, data, thislen);
  }
  dev->rexmit_numbytes += thislen;

  dev->stats_tx_bytes += thislen;
  //todo replace with a little bit of chunking
  for (size_t i = 0; i<thislen; i++) {
    _write_escaped(dev->uart, data[i]);
  }
}

void rethos_end_frame_helper(ethos_t* dev, bool rexmit)
{
    uint16_t cksum = fletcher16_fin(dev->flsum1, dev->flsum2);
    uart_write(dev->uart, _end_frame, 2);
    _write_escaped(dev->uart, cksum & 0xFF);
    _write_escaped(dev->uart, cksum >> 8);
    dev->stats_tx_frames += 1;

    /* Enable retransmission and set the rexmit timer */
    if (rexmit && dev->rexmit_numbytes <= RETHOS_TX_BUF_SZ)
    {
        dev->rexmit_acked = false;
        rexmit_timer.arg = dev;
        xtimer_set(&rexmit_timer, (uint32_t) RETHOS_REXMIT_MICROS);
    }

    if (!irq_is_in())
    {
        mutex_unlock(&dev->out_mutex);
    }
}

void rethos_end_frame(ethos_t *dev)
{
    rethos_end_frame_helper(dev, true);
}

static int _send(netdev2_t *netdev, const struct iovec *vector, unsigned count)
{
    ethos_t * dev = (ethos_t *) netdev;

    rethos_start_frame(dev, NULL, 0, RETHOS_CHANNEL_NETDEV, RETHOS_FRAME_TYPE_DATA);

    /* count total packet length */
    size_t pktlen = iovec_count_total(vector, count);

    while(count--) {
        size_t n = vector->iov_len;
        uint8_t *ptr = vector->iov_base;
        rethos_continue_frame(dev, ptr, n);
        vector++;
    }

    rethos_end_frame(dev);

    return pktlen;
}

static void _get_mac_addr(netdev2_t *encdev, uint8_t* buf)
{
    ethos_t * dev = (ethos_t *) encdev;
    memcpy(buf, dev->mac_addr, 6);
}

static int _recv(netdev2_t *netdev, void *buf, size_t len, void* info)
{
    (void) info;
    ethos_t * dev = (ethos_t *) netdev;

    if (buf) {
        if (len < (int)dev->netdev_packetsz) {
            DEBUG("ethos _recv(): receive buffer too small.\n");
            return -1;
        }

        len = dev->netdev_packetsz;
        dev->netdev_packetsz = 0;

        if ((tsrb_get(&dev->netdev_inbuf, buf, len) != len)) {
            DEBUG("ethos _recv(): inbuf doesn't contain enough bytes.\n");
            return -1;
        }

        return (int)len;
    }
    else {
        return dev->netdev_packetsz;
    }
}

static int _get(netdev2_t *dev, netopt_t opt, void *value, size_t max_len)
{
    int res = 0;

    switch (opt) {
        case NETOPT_ADDRESS:
            if (max_len < ETHERNET_ADDR_LEN) {
                res = -EINVAL;
            }
            else {
                _get_mac_addr(dev, (uint8_t*)value);
                res = ETHERNET_ADDR_LEN;
            }
            break;
        default:
            res = netdev2_eth_get(dev, opt, value, max_len);
            break;
    }

    return res;
}

void rethos_register_handler(ethos_t *dev, rethos_handler_t *handler)
{
  rethos_handler_t *h = dev->handlers;
  handler->_next = NULL;
  if (h == NULL) {
    dev->handlers = handler;
  } else {
    while (h->_next != NULL) {
      h = h->_next;
    }
    h->_next = handler;
  }
}

/* netdev2 interface */
static const netdev2_driver_t netdev2_driver_ethos = {
    .send = _send,
    .recv = _recv,
    .init = _init,
    .isr = _isr,
    .get = _get,
    .set = netdev2_eth_set
};
