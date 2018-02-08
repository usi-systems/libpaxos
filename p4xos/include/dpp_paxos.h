#ifndef _DPDK_PAXOS_H_
#define _DPDK_PAXOS_H_

#ifdef __cplusplus
extern "C" {
#endif

struct paxos_hdr {
	uint16_t msgtype;
	uint32_t inst;
	uint16_t rnd;
	uint16_t vrnd;
	uint16_t acptid;
	uint32_t value_len;
	uint8_t value[MAX_APP_MESSAGE_LEN];
	uint64_t igress_ts;
	uint64_t egress_ts;
} __attribute__((__packed__));

void handle_paxos_message(struct app_lcore_params_worker *lp, struct rte_mbuf *pkt_in);

#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif // _DPDK_PAXOS_H_
