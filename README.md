# myc3 congestion control (Assignment 3, Option 1)

Linux kernel module `myc3.ko`, algorithm name `myc3`. It implements the cc algorithm I specifed in 
Assignment 2 Q3(b), a CUBIC-style time-based growth, with a proactive RTT guard against
bufferbloat (outer layer), and a multiplicative cut on loss (inner layer).

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

## Build, load and run (Ubuntu 26.04.1, kernel 7)

```sh
sudo apt install -y build-essential linux-headers-$(uname -r)
./load.sh                                   # make, insmod, allow myc3
sysctl net.ipv4.tcp_available_congestion_control     # should list myc3 as one of the cc algs
```

From the Assignment 2 folder:

```sh
python3 experiment.py -n 5 -t 10 -i 0.2 -r 3 --seed 42 \
    --algorithms myc3,cubic,reno,bbr -o results_a3```

Check that `cc_used` in `tests.csv` reads `myc3`. Unload with `sudo rmmod myc3`.
If `insmod` says "Key was rejected by service", Secure Boot is blocking unsigned modules.
