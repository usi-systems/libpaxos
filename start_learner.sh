#!/bin/sh

if [[ -z "${LIBPAXOS_BIN}" ]]; then
  echo "${LIBPAXOS_BIN} is unset"
  exit 1
fi

sudo ${LIBPAXOS_BIN}/dppaxos/dpdk_learner -c f -n 4  --socket-mem 256 --file-prefix pg --log-level 7 -- \
--rx "(0,0,0)(1,0,1)" --tx "(0,0)(1,1)" --w "2,3" --pos-lb 33 \
--lpm "192.168.4.95/32=>0;192.168.4.96/32=>0;192.168.4.97/32=>0;192.168.4.98/32=>0;" \
--bsz "(8,8), (8,8), (8,8)" --multi-dbs
