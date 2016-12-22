/**
 * @ingroup     net_gnrc_tcp_freebsd
 * @{
 *
 * @file
 * @brief       Constants used by FreeBSD TCP Protocol logic
 *
 * @author      Sam Kumar <samkumar@berkeley.edu>
 *
 * I created this file to store many of the constants shared by the
 * various files in the FreeBSD protocol logic. I've changed the
 * definitions to be enumerations rather than globals, to save
 * some memory.
 * @}
 */

#ifndef _TCP_CONST_H_
#define _TCP_CONST_H_

#include "../gnrc_tcp_freebsd_internal.h"

#include "tcp_var.h"
#include "tcp_timer.h"

#define MSS_6LOWPAN ((FRAMES_PER_SEG * FRAMECAP_6LOWPAN) - COMPRESSED_IP6HDR_SIZE - sizeof(struct tcphdr))

// I may change some of these flags later
enum tcp_input_consts {
    tcp_keepcnt = TCPTV_KEEPCNT,
    tcp_fast_finwait2_recycle = 0,
    tcprexmtthresh = 3,
    V_drop_synfin = 0,
    V_tcp_do_ecn = 0,
    V_tcp_do_rfc3042 = 0,
    V_path_mtu_discovery = 0,
    V_tcp_delack_enabled = 1,
    V_tcp_initcwnd_segments = 0,
    V_tcp_do_rfc3390 = 0,
    V_tcp_abc_l_var = 2 // this is what was in the original tcp_input.c
};

enum tcp_subr_consts {
    tcp_delacktime = TCPTV_DELACK,
	tcp_keepinit = TCPTV_KEEP_INIT,
	tcp_keepidle = TCPTV_KEEP_IDLE,
	tcp_keepintvl = TCPTV_KEEPINTVL,
	tcp_maxpersistidle = TCPTV_KEEP_IDLE,
	tcp_msl = TCPTV_MSL,
	tcp_rexmit_slop = TCPTV_CPU_VAR,
	tcp_finwait2_timeout = TCPTV_FINWAIT2_TIMEOUT,

    V_tcp_do_rfc1323 = 1,
    V_tcp_v6mssdflt = MSS_6LOWPAN,
    /* Normally, this is used to prevent DoS attacks by sending tiny MSS values in the options. */
    V_tcp_minmss = TCP_MAXOLEN + 1, // Must have enough space for TCP options, and one more byte for data. Default is 216.
    V_tcp_do_sack = 1
};

enum tcp_timer_consts {
//    V_tcp_v6pmtud_blackhole_mss = FRAMECAP_6LOWPAN - sizeof(struct ip6_hdr) - sizeof(struct tcphdr), // Doesn't matter unless blackhole_detect is 1.
    tcp_rexmit_drop_options = 1, // drop options after a few retransmits
    always_keepalive = 1,
};

/*
 * Force a time value to be in a certain range.
 */
#define	TCPT_RANGESET(tv, value, tvmin, tvmax) do { \
	(tv) = (value) + tcp_rexmit_slop; \
	if ((u_long)(tv) < (u_long)(tvmin)) \
		(tv) = (tvmin); \
	if ((u_long)(tv) > (u_long)(tvmax)) \
		(tv) = (tvmax); \
} while(0)

#endif
