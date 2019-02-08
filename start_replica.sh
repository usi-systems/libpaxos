#!/bin/bash

if [[ -z "${LIBPAXOS_BIN}" ]]; then
  echo "${LIBPAXOS_BIN} is unset"
  exit 1
fi


LOG_LEVEL=7
BURST_SIZE=1
OSD=16
TS_INTERVAL=1
CP_INTERVAL=0

mkdir -p /tmp/checkpoints
sudo rm -rf /tmp/checkpoints/*

sudo ${LIBPAXOS_BIN}/dppaxos/dpdk_replica -c ff -n 4  --socket-mem 512 --file-prefix pg --log-level ${LOG_LEVEL} -- \
--rx "(0,0,0)(1,0,0)" --tx "(0,1)(1,1)" --w "4,5,6,7" --pos-lb 43 \
--lpm "192.168.4.95/32=>0;192.168.4.96/32=>0;192.168.4.97/32=>0;192.168.4.98/32=>1;224.0.0.103/32=>1;224.0.0.104/32=>2;224.0.0.105/32=>1;224.0.0.106/32=>2;" \
--bsz "(${BURST_SIZE},${BURST_SIZE}), (${BURST_SIZE},${BURST_SIZE}), (${BURST_SIZE},${BURST_SIZE})" \
--msgtype 2 --osd ${OSD} --multi-dbs \
--inc-inst --cp-interval ${CP_INTERVAL} --dst 224.0.0.103 --ts-interval ${TS_INTERVAL}
