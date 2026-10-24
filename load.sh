#!/bin/sh
# Build and load myc3, then allow it (and BBR) for unprivileged sockets.
# Run on the VM after every boot. Needs: sudo apt install build-essential linux-headers-$(uname -r)
set -e
cd "$(dirname "$0")"
make
if ! lsmod | grep -q '^myc3'; then
    sudo insmod myc3.ko
fi
sudo modprobe tcp_bbr
sudo sysctl -w net.ipv4.tcp_allowed_congestion_control="reno cubic bbr myc3"
uname -r
sysctl net.ipv4.tcp_available_congestion_control net.ipv4.tcp_allowed_congestion_control
