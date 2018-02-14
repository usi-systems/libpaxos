#!/bin/sh
sudo ../build/dppaxos/dpdk_proposer -c f0 -n 4  --socket-mem 256 --file-prefix pr -b 0000:02:00.0 -- \
--rx "(0,0,4)" --tx "(0,5)" --w "6,7" --pos-lb 26 \
--lpm "192.168.4.95/24=>0;192.168.4.96/24=>0;192.168.4.97/24=>0;192.168.4.98/24=>0;" \
--bsz "(8,8), (8,8), (8,8)" --msgtype 2 --osd 16
