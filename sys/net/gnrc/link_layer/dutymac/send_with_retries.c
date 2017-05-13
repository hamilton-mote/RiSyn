#include "net/gnrc.h"
#include "net/gnrc/netdev2.h"
#include "net/netdev2.h"
#include "xtimer.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define NUM_RETRIES SOFTWARE_MAX_FRAME_RETRIES
#define DELAY_MICROS SOFTWARE_FRAME_RETRY_DELAY_MICROS

static xtimer_t retry_timer;
static uint8_t tries_left;
static bool send_in_progress = false;
static void (*send_packet)(gnrc_pktsnip_t*, gnrc_netdev2_t*, bool);
static gnrc_netdev2_t* dev;

static void try_send_packet(void* pkt) {
    send_packet(pkt, dev, true);
}

void retry_init(void) {
    retry_timer.callback = try_send_packet;
}

int send_with_retries(gnrc_pktsnip_t* pkt, int num_retries, void (*send_packet_fn)(gnrc_pktsnip_t*, gnrc_netdev2_t*, bool), gnrc_netdev2_t* gnrc_dutymac_netdev2, bool rexmit) {
    assert(!send_in_progress);
    DEBUG("[send_with_retries] Initiating send...\n");
    retry_timer.arg = pkt;
    if (num_retries < 0) {
        tries_left = NUM_RETRIES;
    } else {
        tries_left = num_retries;
    }
    send_packet = send_packet_fn;
    dev = gnrc_dutymac_netdev2;
    send_in_progress = true;

    send_packet(pkt, dev, rexmit);
    return 0;
}

/* Informs this module that the packet was sent successfully on this try. */
void retry_send_succeeded(void) {
    assert(send_in_progress);
    DEBUG("[send_with_retries] Send successful!\n");
    send_in_progress = false;
}

/* Informs this module that the packet was not sent successfully on this try. */
bool retry_send_failed(void) {
    assert(send_in_progress);
    DEBUG("[send_with_retries] Send failed. %d attempts left...\n", tries_left);
    if (tries_left == 0) {
        send_in_progress = false;
        return false;
    }
    tries_left--;
    xtimer_set(&retry_timer, DELAY_MICROS);
    return true;
}
