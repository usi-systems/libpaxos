#ifndef _DPDK_PAXOS_H_
#define _DPDK_PAXOS_H_

#ifdef __cplusplus
extern "C" {
#endif


#ifndef MAX_APP_MESSAGE_LEN
#define MAX_APP_MESSAGE_LEN 16
#endif
#if (MAX_APP_MESSAGE_LEN >= 1450)
#error "APP_DEFAULT_NUM_ACCEPTORS is too big"
#endif

#define PAXOS_CHOSEN 4
#define PAXOS_RESET  7
#define NEW_COMMAND  8
#define FAST_ACCEPT  9
#define CHECKPOINT   10


enum PAXOS_RETURN_CODE {
        SUCCESS = 0,
        TO_DROP = -1,
        NO_MAJORITY = -2,
        DROP_ORIGINAL_PACKET = -3,
        NO_HANDLER = -4,
		NON_ETHERNET_PACKET = -5,
		NON_UDP_PACKET = -6,
		NON_PAXOS_PACKET = -7
};

struct paxos_hdr {
	uint8_t msgtype;
	uint8_t worker_id;
	uint16_t rnd;
	uint32_t inst;
	uint16_t vrnd;
	uint16_t acptid;
	uint32_t value_len;
	uint8_t value[MAX_APP_MESSAGE_LEN];
	uint64_t igress_ts;
	uint64_t egress_ts;
} __attribute__((__packed__));

size_t get_paxos_offset(void);
int filter_packets(struct rte_mbuf *pkt_in);
void prepare_hw_checksum(struct rte_mbuf *pkt_in, size_t data_size);
#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif // _DPDK_PAXOS_H_
