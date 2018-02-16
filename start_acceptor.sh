#!/bin/sh
sudo ../build/dppaxos/dpdk_acceptor -c f -n 4  --socket-mem 256 --file-prefix ac -- \
--rx "(0,0,0)" --tx "(0,1)" --w "2,3" --pos-lb 26 \
--lpm "192.168.4.95/24=>0;192.168.4.96/24=>0;192.168.4.97/24=>0;192.168.4.98/24=>0;" \
--bsz "(8,8), (8,8), (8,8)"
