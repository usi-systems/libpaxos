#!/bin/sh

mkdir -p /tmp/checkpoints
sudo rm -rf /tmp/checkpoints/*

sudo ../build/dppaxos/dpdk_replica -c f -n 4  --socket-mem 256 --file-prefix pg --log-level 7 -- \
--rx "(0,0,0)(1,0,1)" --tx "(0,0)(1,1)" --w "2,3" --pos-lb 33 \
--lpm "192.168.4.95/32=>0;192.168.4.96/32=>0;192.168.4.97/32=>0;192.168.4.98/32=>1;" \
--bsz "(8,8), (8,8), (8,8)" --msgtype 2 --osd 16 --multi-dbs \
--inc-inst --all-ports --cp-interval 51200
