#include "net/gnrc.h"
#include "net/gnrc/netdev2.h"

void retry_init(void);
/* Always returns 0. */
void send_with_retries(gnrc_pktsnip_t* pkt, int num_retries, void (*send_packet_fn)(gnrc_pktsnip_t*, gnrc_netdev2_t*, bool), gnrc_netdev2_t* gnrc_dutymac_netdev2, bool rexmit);
void retry_send_succeeded(void);
bool retry_send_failed(void);

void csma_init(void);
/* Always returns 0. */
void send_with_csma(gnrc_pktsnip_t* pkt, void (*send_packet_fn)(gnrc_pktsnip_t*, gnrc_netdev2_t*, bool), gnrc_netdev2_t* gnrc_dutymac_netdev2, bool rexmit, bool skip_first_backoff);
void csma_send_succeeded(void);
bool csma_send_failed(void);
