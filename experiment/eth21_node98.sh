#!/bin/sh
sudo $RTE_SDK/tools/dpdk_nic_bind.py --bind=ixgbe 01:00.0
sudo $RTE_SDK/tools/dpdk_nic_bind.py --bind=ixgbe 01:00.1
sudo ip link set dev eth21 up
sudo ip addr add 192.168.4.98/24 dev eth21
sudo ip link set eth21 multicast on
sudo ip route add 224.0.0.0/4 dev eth21
sudo bash -c 'echo 1 > /proc/sys/net/ipv4/ip_forward'
sudo ethtool -K eth21 tx off rx off gso off gro off
