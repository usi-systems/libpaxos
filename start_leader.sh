#!/bin/sh

if [[ -z "${LIBPAXOS_BIN}" ]]; then
  echo "${LIBPAXOS_BIN} is unset"
  exit 1
fi


sudo $LIBPAXOS_BIN/dppaxos/dpdk_leader -c ff -n 4  --socket-mem 256 --file-prefix ld --log-level 7 -- \
--rx "(0,0,0)(1,0,0)" --tx "(0,1)(1,1)" --w "2,3,4,5" --pos-lb 43 \
--lpm "192.168.4.0/24=>1;224.0.0.0/4=>2;" \
--bsz "(1,1), (1,1), (1,1)" --msgtype 2 --osd 16 --multi-dbs
