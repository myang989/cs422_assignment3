# myc3 congestion control (Assignment 3, Option 1)

Linux kernel module for my custom cc algo: `myc3`. It implements an improved version of the cc algorithm I specifed in 
Assignment 2 Q3(b), a CUBIC-style time-based growth, with a proactive RTT guard against
bufferbloat (outer layer), and a multiplicative cut on loss (inner layer).

## Build, load and run (on Ubuntu 26.04.1, Linux kernel version 7.0)

```sh
sudo apt install -y build-essential linux-headers-$(uname -r)
./load.sh                                   # make, insmod, allow myc3
sysctl net.ipv4.tcp_available_congestion_control     # should list myc3 as one of the cc algs
```

To run expirments with my myc3, from the Assignment 2 folder:

```sh
python3 experiment.py -n 5 -t 10 -i 0.2 -r 3 --seed 42 \
    --algorithms myc3,cubic,reno,bbr -o results_a3
```

Check that `cc_used` in `tests.csv` reads `myc3`. You unload with `sudo rmmod myc3`.

## AI Usage
AI tooling like Claude and Google search AI were used to help generate and understand some of the more lower level components of the implmentation. Also I used AI assitance to figure out how to build. However the algorithm pseudocode I proudly designed myself.