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
