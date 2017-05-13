#include "net/gnrc.h"
#include "net/gnrc/netdev2.h"
#include "net/netdev2.h"
#include "random.h"
#include "xtimer.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define MIN_BE SOFTWARE_CSMA_MIN_BACKOFF_EXP
#define MAX_BE SOFTWARE_CSMA_MAX_BACKOFF_EXP

#if HARDWARE_CSMA_EN

#define MAX_TRIES 1
#define BACKOFF_PERIOD_MICROS 100

#else

#define MAX_TRIES SOFTWARE_CSMA_MAX_TRIES // Back off 5 times before giving up
#define BACKOFF_PERIOD_MICROS SOFTWARE_CSMA_BACKOFF_MICROS

#endif

static xtimer_t backoff_timer;
static uint8_t num_tries;
static bool send_in_progress = false;
static void (*send_packet)(gnrc_pktsnip_t*, gnrc_netdev2_t*, bool);
static gnrc_netdev2_t* dev;
static bool is_rexmit;

static void try_send_packet(void* pkt) {
    send_packet(pkt, dev, is_rexmit);
    is_rexmit = false;
}

void backoff_and_send(void) {
    uint8_t be = MIN_BE + num_tries;
    if (be > MAX_BE) {
        be = MAX_BE;
    }
    uint32_t max_possible_backoff = ((uint32_t) BACKOFF_PERIOD_MICROS) << be;
    if (max_possible_backoff == 0) {
        try_send_packet(backoff_timer.arg);
    } else {
        uint32_t micros_to_wait = random_uint32_range(0, max_possible_backoff);
        xtimer_set(&backoff_timer, micros_to_wait);
    }
}

int send_with_csma(gnrc_pktsnip_t* pkt, void (*send_packet_fn)(gnrc_pktsnip_t*, gnrc_netdev2_t*, bool), gnrc_netdev2_t* gnrc_dutymac_netdev2, bool rexmit, bool skip_first_backoff) {
    assert(!send_in_progress);
    backoff_timer.arg = pkt;
    num_tries = 0;
    send_packet = send_packet_fn;
    dev = gnrc_dutymac_netdev2;
    is_rexmit = rexmit;
    send_in_progress = true;

    if (skip_first_backoff) {
        try_send_packet(pkt);
    } else {
        backoff_and_send();
    }
    return 0;
}

void csma_send_succeeded(void) {
    send_in_progress = false;
}

bool csma_send_failed(void) {
    num_tries++;
    if (num_tries >= MAX_TRIES) {
        send_in_progress = false;
        return false;
    }
    backoff_and_send();
    return true;
}

void csma_init(void) {
    backoff_timer.callback = try_send_packet;
}
