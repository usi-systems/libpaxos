#!/bin/bash


if [ "$EUID" -ne 0 ] ;  then
	echo "Please run as root"
	echo "sudo -E ./setup.sh"
	exit -1
fi

if [ -z ${RTE_SDK} ]; then
	echo "Please set \$RTE_SDK variable"
	echo "sudo -E ./setup.sh"
	exit -1
fi

modprobe uio
insmod $RTE_SDK/$RTE_TARGET/kmod/igb_uio.ko
$RTE_SDK/usertools/dpdk-devbind.py --status

# if [ $# -ne 2 ] ; then
# 	echo "Please provide the interface name and PCI address"
# 	echo "e.g, sudo -E ./setup.sh eth2 01:00.0"
# 	exit -1
# fi
sudo mkdir -p /mnt/huge
sudo mount -t hugetlbfs nodev /mnt/huge/
echo 2048 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

sudo ip link set dev eth0 down
$RTE_SDK/usertools/dpdk-devbind.py --bind=igb_uio eth0
sudo ip link set dev eth1 down
$RTE_SDK/usertools/dpdk-devbind.py --bind=igb_uio eth1
$RTE_SDK/usertools/dpdk-devbind.py --status
