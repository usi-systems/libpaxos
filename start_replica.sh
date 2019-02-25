#!/bin/bash

if [[ -z "${LIBPAXOS_BIN}" ]]; then
  echo "${LIBPAXOS_BIN} is unset"
  exit 1
fi


LOG_LEVEL=7
BURST_SIZE=1
OSD=16
TS_INTERVAL=1
cpi=0
cli_addr=192.168.4.95
leader_ip=192.168.4.98
acceptor_ip=224.0.0.103
src_addr=192.168.4.98
res_port=0
max_inst=1000000
node_id=1
resp=--resp
leader=--leader
baseline=--baseline
num_ac=3
mkdir -p /tmp/checkpoints
sudo rm -rf /tmp/checkpoints/*

CMD="${LIBPAXOS_BIN}/dppaxos/dpdk_replica -c ff -n 4  --socket-mem 512 --file-prefix pg --log-level ${LOG_LEVEL} -- \
--rx \"(0,0,0)\" --tx \"(0,1)\" --w \"4,5,6,7\" --pos-lb 43 \
--lpm \"192.168.4.0/24=>${res_port};224.0.0.0/4=>1;\" \
--bsz \"(${BURST_SIZE},${BURST_SIZE}), (${BURST_SIZE},${BURST_SIZE}), (${BURST_SIZE},${BURST_SIZE})\" \
--multi-dbs --dst 224.0.0.103:9081 --cliaddr ${cli_addr}:27461 \
--pri ${leader_ip}:9081 --acc-addr ${acceptor_ip}:9081 ${run_prepare}\
--src ${src_addr}:37452 --port ${res_port} --num-ac ${num_ac} --cp-interval ${cpi} \
--max ${max_inst} ${baseline} --node-id ${node_id} ${resp} ${leader}"

echo $CMD
