#!/bin/sh

mkdir -p /tmp/checkpoints
sudo rm -rf /tmp/checkpoints/*

sudo ../build/dppaxos/dpdk_replica -c ff -n 4  --socket-mem 256 --file-prefix pg --log-level 7 -- \
--rx "(0,0,0)(1,0,0)" --tx "(0,1)(1,1)" --w "4,5,6,7" --pos-lb 67 \
--lpm "192.168.4.95/32=>0;192.168.4.96/32=>0;192.168.4.97/32=>0;192.168.4.98/32=>1;224.0.0.103/32=>1;224.0.0.104/32=>2;224.0.0.105/32=>1;224.0.0.106/32=>2;" \
--bsz "(2,2), (2,2), (2,2)" --msgtype 2 --osd 16 --multi-dbs \
--inc-inst --cp-interval 0 --dst 224.0.0.103
