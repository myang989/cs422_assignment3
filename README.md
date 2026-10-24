# myc3 congestion control (Assignment 3, Option 1)

Linux kernel module `tcp_myc3.ko`, algorithm name `myc3`. It implements the
Assignment 2 Q3(b) design: CUBIC-style time-based growth, a proactive RTT guard against
bufferbloat (outer layer), and a multiplicative cut on loss (inner layer).

## Pseudocode as implemented

```
constants: BETA = 0.7, GUARD_CUT = 0.9, RTT_FACTOR = 1.5, C = 0.4
state:     cwnd = 10, ssthresh = infinity, W_max = 10, epoch = none

on every ACK with an RTT sample r:
    round_min_rtt = min(round_min_rtt, r)

once per RTT (when the ACK covers the data outstanding at the round start):
    if round_min_rtt > RTT_FACTOR * min_rtt:        # outer layer: bufferbloat guard
        cwnd = max(GUARD_CUT * cwnd, 2)
        ssthresh = cwnd                             # leaves slow start if still in it
        epoch = none                                # restart the cubic curve
    round_min_rtt = infinity

on every ACK:
    if cwnd < ssthresh:                             # slow start
        cwnd += 1 per ACKed segment                 # doubles every RTT
    else:
        if epoch == none:                           # start a growth epoch
            epoch_start = now
            if W_max > cwnd: K = cbrt((W_max - cwnd) / C);  origin = W_max
            else:            K = 0;                         origin = cwnd
        target = origin + C * (now - epoch_start - K)^3
        if target > cwnd: grow toward target over about one RTT (at most +50% per RTT)
        else:             cwnd += 1 / cwnd per ACK  # linear fallback: +1 per RTT

on loss (3 duplicate ACKs / SACK, or timeout):     # inner layer
    W_max = cwnd
    ssthresh = max(BETA * cwnd, 2);  cwnd = ssthresh after recovery (1 after a timeout)
    epoch = none
```

## Changes from the original Q3(b) pseudocode

| Original | Implemented | Why |
|---|---|---|
| Guard checked on every ACK | Once per RTT | With about 800 ACKs per RTT, `cwnd *= 0.9` per ACK would cut cwnd to the minimum within one round trip. |
| Guard compares each ACK's RTT | Compares the smallest RTT sample in the round | Wi-Fi jitter raised single samples past 1.5 × min_rtt (SRTT swung 130–220 ms on a 130 ms path). Only a lasting queue raises the round's minimum. |
| No slow start | Slow start until the first loss or guard trip | Cubic growth from cwnd = 10 needs about 15 s to reach the ~800-segment windows these paths need; the A3 tests last 10 s. |
| K = cbrt(W_max·(1−β)/C) | K = cbrt((W_max − cwnd)/C), using cwnd at the epoch start | The same value after a 0.7 cut, but it also places the curve correctly after a guard cut (0.9) or at the end of slow start. If cwnd ≥ W_max, the curve starts at cwnd with K = 0. |
| cwnd = target immediately | Close the gap over about one RTT, at most +50% per RTT | Avoids sending a burst of packets in a single ACK. |
| Guard cut leaves ssthresh alone | ssthresh = cwnd after a guard cut | Otherwise cwnd < ssthresh would put the connection back into slow start. |
| Timeout handled like 3 duplicate ACKs | Kernel sets cwnd = 1 after a timeout; 0.7 × cwnd becomes ssthresh | The kernel's RTO handling resets cwnd; the algorithm only supplies ssthresh. |

## Initial state and parameters

- Initial state: cwnd = 10 segments (kernel default), ssthresh = infinity, W_max = 10, no epoch,
  first round ends at `snd_nxt` when the connection is established.
- `BG_BETA 717/1024` (0.7), `BG_GUARD_CUT 922/1024` (0.9), threshold `3/2` × min_rtt,
  `C = 4/10`, `t − K` clamped to 100 s. Fixed-point arithmetic because the kernel has no floating point.
- min_rtt is the kernel's windowed minimum (`tcp_min_rtt()`, 5-minute window).

## Kernel hooks

| Hook | Called | Does |
|---|---|---|
| `init` | Connection established | Sets W_max and starts the first round |
| `pkts_acked` | Every ACK of new data | Tracks the smallest RTT sample in the round |
| `cong_avoid` | Every ACK that may raise cwnd (not during fast recovery) | Runs the once-per-RTT guard, slow start, and cubic or linear growth |
| `ssthresh` | Fast retransmit and timeout | Saves W_max, returns 0.7 × cwnd |
| `undo_cwnd` | Loss turned out to be spurious | Kernel's `tcp_reno_undo_cwnd` |

## Build, load and run (Ubuntu 24.04, kernel 6.8)

```sh
sudo apt install -y build-essential linux-headers-$(uname -r)
./load.sh                                   # make, insmod, allow myc3
sysctl net.ipv4.tcp_available_congestion_control     # should list myc3
```

From the Assignment 2 folder:

```sh
python3 experiment.py --dest-file results/<a2_run_id>/chosen_servers.csv \
    -t 10 -i 0.2 -r 3 --algorithms myc3,cubic,reno,bbr -o results_a3
```

Check that `cc_used` in `tests.csv` reads `myc3`. Unload with `sudo rmmod tcp_myc3`.
If `insmod` says "Key was rejected by service", Secure Boot is blocking unsigned modules.
