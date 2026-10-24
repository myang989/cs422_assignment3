// SPDX-License-Identifier: GPL-2.0
/*
 * myc3: [M]y [C]ustom [C]ongestion [C]ontrol Algorithm
 * (CS 422 Assignment 3, Option 1).
 *
 *  Algorithm Components
 *
 *  Slow Start:
 *   - Doubles cwnd every RTT until the first loss or guard trip.
 *   - Enter congestion avoidance mode following AIMD
 *  Additive Increase:
 *   - cwnd follows the CUBIC time increase of W(t) = C*(t - K)^3 + W_max,
 *     where t is the time since the last congestion event; below the curve it falls back to +1 segment per RTT.
 *  Multiplicative Decrease:
 *   - Proactive guard (outer layer) checked once per RTT: if the smallest RTT
 *     sample in the round exceeds 1.5 * min_rtt, cwnd *= 0.9 and a new epoch starts.
 *   - Reactive loss fallback (inner layer): W_max = cwnd, cwnd *= 0.7, new epoch.
 */

#include <linux/module.h>
#include <linux/math64.h>
#include <net/tcp.h>

#define MYC3_BETA 717      /* 717/1024 = 0.7 size of the cut after a loss */
#define MYC3_GUARD_CUT 922 /* 922/1024 = 0.9 size of cut when the guard trips */
#define MYC3_RTT_NUM 3     /* guard threshold = min_rtt * 3/2 */
#define MYC3_RTT_DEN 2
#define MYC3_C_NUM 4 /* C = 4/10 = 0.4 segments/s^3 */
#define MYC3_C_DEN 10
#define MYC3_MAX_T_MS 100000 /* clamp t - K to 100 s to avoid overflow */

/* Each connection will get 104 bytes of private storage for its cc algo.
On modern linux its 104 bytes while older kernels use 64. */
struct myc3
{
    u64 epoch_start_us;   /* when the current growth epoch (cubic curve) started; 0 means start at next ACK */
    u32 w_max;            /* cwnd at the last loss */
    u32 origin;           /* W(K): the plateau the cubic curve passes through */
    u32 k_ms;             /* time from epoch start to reach origin */
    u32 round_end_seq;    /* the round ends when this sequence number is ACKed */
    u32 round_min_rtt_us; /* running min rtt in the current round */
};

/* Integer cube root - largest k with k^3 <= x (binary search; runs once per epoch). */
static u32 myc3_cbrt(u64 x)
{
    u32 lo = 0, hi = 2097151; /* 2097151^3 < 2^64 */

    while (lo < hi)
    {
        u32 mid = lo + (hi - lo + 1) / 2;

        if ((u64)mid * mid * mid <= x)
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo;
}

/* Begin measuring a new round. A round is the time it takes to send a full cwnd.
The round ends when snd_nxt is ACKed (last packet of the window is ACKED); reset the round's min RTT. */
static void myc3_start_round(struct sock *sk)
{
    // get pointers to tcp state and our cc algo
    struct tcp_sock *tp = tcp_sk(sk);
    struct myc3 *ca = inet_csk_ca(sk);

    // saves next sequence number
    ca->round_end_seq = tp->snd_nxt;
    ca->round_min_rtt_us = ~0U;
}

// This tells the os which of the above functions to call for each event
static struct tcp_congestion_ops tcp_myc3 __read_mostly = {
    .init = myc3_init,
    .ssthresh = myc3_ssthresh,
    .cong_avoid = myc3_cong_avoid,
    .pkts_acked = myc3_pkts_acked,
    .undo_cwnd = tcp_reno_undo_cwnd,
    .owner = THIS_MODULE,
    .name = "myc3",
};

static int __init myc3_register(void)
{
    BUILD_BUG_ON(sizeof(struct myc3) > ICSK_CA_PRIV_SIZE);
    return tcp_register_congestion_control(&tcp_myc3);
}

static void __exit myc3_unregister(void)
{
    tcp_unregister_congestion_control(&tcp_myc3);
}

// myc3_registers runs at insmod, adds myc3 to kernel list of cc algos
module_init(myc3_register);
// remove myc3 from list
module_exit(myc3_unregister);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("myc3: CUBIC-style growth with a proactive RTT guard and loss fallback");
