#ifndef _DPDK_PAXOS_H_
#define _DPDK_PAXOS_H_

/* Paxos logic */
#ifndef APP_DEFAULT_NUM_ACCEPTORS
#define APP_DEFAULT_NUM_ACCEPTORS 1
#endif
#if (APP_DEFAULT_NUM_ACCEPTORS >= 7)
#error "APP_DEFAULT_NUM_ACCEPTORS is too big"
#endif

#define P4XOS_PORT 9081

#define APP_DEFAULT_IP_SRC_ADDR "192.168.4.95:48153"
#define APP_DEFAULT_IP_LEADER_ADDR "192.168.4.98:9081"
#define APP_DEFAULT_IP_ACCEPTOR_ADDR "224.0.0.103:9081"
#define APP_DEFAULT_IP_LEARNER_ADDR "224.0.0.104:9081"
#define APP_DEFAULT_MESSAGE_TYPE 0x0003
#define APP_DEFAULT_FALSE 0
#define APP_DEFAULT_TX_PORT 0
#define APP_DEFAULT_NODE_ID 0
#define APP_DEFAULT_CHECKPOINT_INTERVAL 0
#define APP_DEFAULT_TS_INTERVAL 4
#define APP_DEFAULT_OUTSTANDING	8
#define APP_DEFAULT_MAX_INST	24000000
#define APP_DEFAULT_SENDING_RATE 10000
#define APP_PREEXEC_WINDOW 0
#define FILENAME_LENGTH 128
#define CHUNK_SIZE 4096


#ifndef MAX_APP_MESSAGE_LEN
#define MAX_APP_MESSAGE_LEN 4
#endif
#if (MAX_APP_MESSAGE_LEN >= 1450)
#error "APP_DEFAULT_NUM_ACCEPTORS is too big"
#endif

#define PAXOS_VALUE_SIZE 8

#define PAXOS_CHOSEN       0x04
#define PAXOS_RESET        0x07
#define NEW_COMMAND        0x08
#define FAST_ACCEPT        0x09
#define CHECKPOINT         0x0A
#define LEARNER_PREPARE    0x20
#define LEARNER_CHECKPOINT 0x22
#define SET_INSTANCE 0x30

#ifdef __cplusplus
extern "C" {
#endif


enum PAXOS_RETURN_CODE {
  PAXOS_PACKET = 1,
  SUCCESS = 0,
  TO_DROP = -1,
  NO_MAJORITY = -2,
  DROP_ORIGINAL_PACKET = -3,
  NO_HANDLER = -4,
  NON_ETHERNET_PACKET = -5,
  NON_UDP_PACKET = -6,
  NON_PAXOS_PACKET = -7,
  NO_ACCEPT_INSTANCE = -8
};

struct paxos_hdr {
    uint8_t msgtype;
    uint8_t worker_id;
    uint16_t rnd;
    uint32_t inst;
    uint16_t log_index;
    uint16_t vrnd;
    uint16_t acptid;
    uint16_t reserved;
    uint64_t value;
    uint32_t request_id;
    uint64_t igress_ts;
} __attribute__((__packed__));


struct p4xos_configuration {
	uint8_t num_acceptors;
	uint8_t multi_dbs;
	uint8_t msgtype;
	uint16_t tx_port;
	uint16_t node_id;
	struct sockaddr_in client;
	struct sockaddr_in mine;
	struct sockaddr_in paxos_leader;
	struct sockaddr_in acceptor_addr;
	struct sockaddr_in learner_addr;
	uint32_t osd;
	uint32_t max_inst;
	uint8_t inc_inst;
	uint8_t reset_inst;
	uint8_t baseline;
	uint8_t drop;
	uint8_t leader;
	uint8_t measure_latency;
	uint8_t run_prepare;
	uint8_t respond_to_client;
	uint32_t checkpoint_interval;
	uint32_t ts_interval;
	uint32_t rate;
	uint32_t preexec_window;
    char *app_config;
};

size_t get_paxos_offset(void);
void print_paxos_hdr(struct paxos_hdr *paxos_hdr);
int filter_packets(struct rte_mbuf *pkt_in);
void prepare_hw_checksum(struct rte_mbuf *pkt_in, size_t data_size);
void set_paxos_hdr(struct paxos_hdr *px, uint8_t msgtype, uint32_t inst,
                          uint16_t rnd, uint8_t worker_id, uint16_t acptid,
                          uint32_t request_id, uint64_t igress_ts, char *value, int size);
int prepare_handler(struct paxos_hdr *paxos_hdr, void *arg);
int accept_handler(struct paxos_hdr *paxos_hdr, void *arg);
int learner_checkpoint_handler(struct paxos_hdr *paxos_hdr, void *arg);

#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif // _DPDK_PAXOS_H_
