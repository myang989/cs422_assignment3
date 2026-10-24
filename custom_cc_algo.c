/*
 * myc3: My Custom Congestion Control Algorithm
 * (CS 422 Assignment 3, Option 1).
 *
 *  Algorithm Components
 *   - Slow start doubles cwnd every RTT until the first loss or guard trip.
 *   - Enters congestion avoidance mode following AIMD
 *   - Additive Increase: cwnd follows the CUBIC time increase of W(t) = C*(t - K)^3 + W_max,
 *     where t is the time since the last congestion event; below the curve it falls back to +1 segment per RTT.
 *   - Proactive guard (outer layer) checked once per RTT: if the smallest RTT
 *     sample in the round exceeds 1.5 * min_rtt, cwnd *= 0.9 and a new epoch starts.
 *   - Reactive loss fallback (inner layer): W_max = cwnd, cwnd *= 0.7, new epoch.
 */