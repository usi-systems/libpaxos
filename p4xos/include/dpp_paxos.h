#ifndef _DPDK_PAXOS_H_
#define _DPDK_PAXOS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define PAXOS_RESET 0x07
#define PAXOS_BEGIN	0xBB
#define PAXOS_FINISHED	0xFF
#define PAXOS_ACCEPT_FAST	0x08

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


#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif // _DPDK_PAXOS_H_
