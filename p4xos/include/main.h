/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_lpm.h>
#include <rte_timer.h>
#include <rte_sched.h>

#include "paxos.h"
#include "dpp_paxos.h"
/* Logical cores */
#ifndef APP_MAX_SOCKETS
#define APP_MAX_SOCKETS 2
#endif

#ifndef APP_MAX_LCORES
#define APP_MAX_LCORES       RTE_MAX_LCORE
#endif

#ifndef APP_MAX_NIC_PORTS
#define APP_MAX_NIC_PORTS    RTE_MAX_ETHPORTS
#endif

#ifndef APP_MAX_RX_QUEUES_PER_NIC_PORT
#define APP_MAX_RX_QUEUES_PER_NIC_PORT 128
#endif

#ifndef APP_MAX_TX_QUEUES_PER_NIC_PORT
#define APP_MAX_TX_QUEUES_PER_NIC_PORT 128
#endif

#ifndef APP_MAX_IO_LCORES
#if (APP_MAX_LCORES > 16)
#define APP_MAX_IO_LCORES 16
#else
#define APP_MAX_IO_LCORES APP_MAX_LCORES
#endif
#endif
#if (APP_MAX_IO_LCORES > APP_MAX_LCORES)
#error "APP_MAX_IO_LCORES is too big"
#endif

#ifndef APP_MAX_NIC_RX_QUEUES_PER_IO_LCORE
#define APP_MAX_NIC_RX_QUEUES_PER_IO_LCORE 16
#endif

#ifndef APP_MAX_NIC_TX_PORTS_PER_IO_LCORE
#define APP_MAX_NIC_TX_PORTS_PER_IO_LCORE 16
#endif
#if (APP_MAX_NIC_TX_PORTS_PER_IO_LCORE > APP_MAX_NIC_PORTS)
#error "APP_MAX_NIC_TX_PORTS_PER_IO_LCORE too big"
#endif

#ifndef APP_MAX_WORKER_LCORES
#if (APP_MAX_LCORES > 16)
#define APP_MAX_WORKER_LCORES 16
#else
#define APP_MAX_WORKER_LCORES APP_MAX_LCORES
#endif
#endif
#if (APP_MAX_WORKER_LCORES > APP_MAX_LCORES)
#error "APP_MAX_WORKER_LCORES is too big"
#endif


/* Mempools */
#ifndef APP_DEFAULT_MBUF_DATA_SIZE
#define APP_DEFAULT_MBUF_DATA_SIZE  RTE_MBUF_DEFAULT_BUF_SIZE
#endif

#ifndef APP_DEFAULT_MEMPOOL_BUFFERS
#define APP_DEFAULT_MEMPOOL_BUFFERS   8192 * 4
#endif

#ifndef APP_DEFAULT_MEMPOOL_CACHE_SIZE
#define APP_DEFAULT_MEMPOOL_CACHE_SIZE  256
#endif

/* LPM Tables */
#ifndef APP_MAX_LPM_RULES
#define APP_MAX_LPM_RULES 1024
#endif

/* NIC RX */
#ifndef APP_DEFAULT_NIC_RX_RING_SIZE
#define APP_DEFAULT_NIC_RX_RING_SIZE 1024
#endif

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#ifndef APP_DEFAULT_NIC_RX_PTHRESH
#define APP_DEFAULT_NIC_RX_PTHRESH  8
#endif

#ifndef APP_DEFAULT_NIC_RX_HTHRESH
#define APP_DEFAULT_NIC_RX_HTHRESH  8
#endif

#ifndef APP_DEFAULT_NIC_RX_WTHRESH
#define APP_DEFAULT_NIC_RX_WTHRESH  4
#endif

#ifndef APP_DEFAULT_NIC_RX_FREE_THRESH
#define APP_DEFAULT_NIC_RX_FREE_THRESH  64
#endif

#ifndef APP_DEFAULT_NIC_RX_DROP_EN
#define APP_DEFAULT_NIC_RX_DROP_EN 0
#endif

/* NIC TX */
#ifndef APP_DEFAULT_NIC_TX_RING_SIZE
#define APP_DEFAULT_NIC_TX_RING_SIZE 1024
#endif

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#ifndef APP_DEFAULT_NIC_TX_PTHRESH
#define APP_DEFAULT_NIC_TX_PTHRESH  36
#endif

#ifndef APP_DEFAULT_NIC_TX_HTHRESH
#define APP_DEFAULT_NIC_TX_HTHRESH  0
#endif

#ifndef APP_DEFAULT_NIC_TX_WTHRESH
#define APP_DEFAULT_NIC_TX_WTHRESH  0
#endif

#ifndef APP_DEFAULT_NIC_TX_FREE_THRESH
#define APP_DEFAULT_NIC_TX_FREE_THRESH  0
#endif

#ifndef APP_DEFAULT_NIC_TX_RS_THRESH
#define APP_DEFAULT_NIC_TX_RS_THRESH  0
#endif

/* Software Rings */
#ifndef APP_DEFAULT_RING_RX_SIZE
#define APP_DEFAULT_RING_RX_SIZE 1024
#endif

#ifndef APP_DEFAULT_RING_TX_SIZE
#define APP_DEFAULT_RING_TX_SIZE 1024
#endif

/* Bursts */
#ifndef APP_MBUF_ARRAY_SIZE
#define APP_MBUF_ARRAY_SIZE   512
#endif

#ifndef APP_DEFAULT_BURST_SIZE_IO_RX_READ
#define APP_DEFAULT_BURST_SIZE_IO_RX_READ  144
#endif
#if (APP_DEFAULT_BURST_SIZE_IO_RX_READ > APP_MBUF_ARRAY_SIZE)
#error "APP_DEFAULT_BURST_SIZE_IO_RX_READ is too big"
#endif

#ifndef APP_DEFAULT_BURST_SIZE_IO_RX_WRITE
#define APP_DEFAULT_BURST_SIZE_IO_RX_WRITE  144
#endif
#if (APP_DEFAULT_BURST_SIZE_IO_RX_WRITE > APP_MBUF_ARRAY_SIZE)
#error "APP_DEFAULT_BURST_SIZE_IO_RX_WRITE is too big"
#endif

#ifndef APP_DEFAULT_BURST_SIZE_IO_TX_READ
#define APP_DEFAULT_BURST_SIZE_IO_TX_READ  144
#endif
#if (APP_DEFAULT_BURST_SIZE_IO_TX_READ > APP_MBUF_ARRAY_SIZE)
#error "APP_DEFAULT_BURST_SIZE_IO_TX_READ is too big"
#endif

#ifndef APP_DEFAULT_BURST_SIZE_IO_TX_WRITE
#define APP_DEFAULT_BURST_SIZE_IO_TX_WRITE  144
#endif
#if (APP_DEFAULT_BURST_SIZE_IO_TX_WRITE > APP_MBUF_ARRAY_SIZE)
#error "APP_DEFAULT_BURST_SIZE_IO_TX_WRITE is too big"
#endif

#ifndef APP_DEFAULT_BURST_SIZE_WORKER_READ
#define APP_DEFAULT_BURST_SIZE_WORKER_READ  144
#endif
#if ((2 * APP_DEFAULT_BURST_SIZE_WORKER_READ) > APP_MBUF_ARRAY_SIZE)
#error "APP_DEFAULT_BURST_SIZE_WORKER_READ is too big"
#endif

#ifndef APP_DEFAULT_BURST_SIZE_WORKER_WRITE
#define APP_DEFAULT_BURST_SIZE_WORKER_WRITE  144
#endif
#if (APP_DEFAULT_BURST_SIZE_WORKER_WRITE > APP_MBUF_ARRAY_SIZE)
#error "APP_DEFAULT_BURST_SIZE_WORKER_WRITE is too big"
#endif

/* Load balancing logic */
#ifndef APP_DEFAULT_IO_RX_LB_POS
#define APP_DEFAULT_IO_RX_LB_POS 29
#endif
#if (APP_DEFAULT_IO_RX_LB_POS >= 64)
#error "APP_DEFAULT_IO_RX_LB_POS is too big"
#endif


#define MAX_SCHED_SUBPORTS 1
#define MAX_SCHED_PIPES 1
#undef RTE_SCHED_PIPE_PROFILES_PER_PORT
#define RTE_SCHED_PIPE_PROFILES_PER_PORT 1
#define SCHED_PORT_QUEUE_SIZE 64

#define TIMER_RESOLUTION_CYCLES 16000000ULL /* around 10ms at 1.6 Ghz */

#define STAT_PERIOD 1		/* get stat every 1/STAT_PERIOD (s) */
#define RESUBMIT_TIMEOUT 1 /* Client resubmit every 1/RESUBMIT_TIMEOUT (s) */
#define LEADER_CHECK_TIMEOUT 20 /* Client check closed prepare/accept every 1/LEADER_CHECK_TIMEOUT (s) */
#define MAX_N_CONCURRENT_REQUEST 8

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*deliver_cb)(unsigned int, unsigned int, char* value, size_t size, void* arg);
typedef int (*worker_cb)(struct rte_mbuf *pkt_in, void *arg);
typedef int (*recv_cb)(char *buffer, size_t len, uint32_t worker_id, struct sockaddr_in *from);


struct app_mbuf_array {
	struct rte_mbuf *array[APP_MBUF_ARRAY_SIZE];
	uint32_t n_mbufs;
};

enum app_lcore_type {
	e_APP_LCORE_DISABLED = 0,
	e_APP_LCORE_IO,
	e_APP_LCORE_WORKER
};

struct app_lcore_params_io {
	/* I/O RX */
	struct {
		/* NIC */
		struct {
			uint16_t port;
			uint8_t queue;
		} nic_queues[APP_MAX_NIC_RX_QUEUES_PER_IO_LCORE];
		uint32_t n_nic_queues;

		/* Rings */
		struct rte_ring *rings[APP_MAX_WORKER_LCORES];
		uint32_t n_rings;

		/* Internal buffers */
		struct app_mbuf_array mbuf_in;
		struct app_mbuf_array mbuf_out[APP_MAX_WORKER_LCORES];
		uint8_t mbuf_out_flush[APP_MAX_WORKER_LCORES];

		/* Stats */
		uint32_t nic_queues_count[APP_MAX_NIC_RX_QUEUES_PER_IO_LCORE];
		uint32_t nic_queues_iters[APP_MAX_NIC_RX_QUEUES_PER_IO_LCORE];
		uint32_t rings_count[APP_MAX_WORKER_LCORES];
		uint32_t rings_drop[APP_MAX_WORKER_LCORES];
		uint32_t rings_iters[APP_MAX_WORKER_LCORES];
	} rx;

	/* I/O TX */
	struct {
		/* Rings */
		struct rte_ring *rings[APP_MAX_NIC_PORTS][APP_MAX_WORKER_LCORES];

		/* NIC */
		uint16_t nic_ports[APP_MAX_NIC_TX_PORTS_PER_IO_LCORE];
		uint32_t n_nic_ports;

		/* Sched Port */
		struct rte_sched_port *sched_port;

		/* Internal buffers */
		struct app_mbuf_array mbuf_out[APP_MAX_NIC_TX_PORTS_PER_IO_LCORE];
		uint8_t mbuf_out_flush[APP_MAX_NIC_TX_PORTS_PER_IO_LCORE];

		/* Stats */
		uint32_t rings_count[APP_MAX_NIC_PORTS][APP_MAX_WORKER_LCORES];
		uint32_t rings_iters[APP_MAX_NIC_PORTS][APP_MAX_WORKER_LCORES];
		uint32_t nic_ports_count[APP_MAX_NIC_TX_PORTS_PER_IO_LCORE];
		uint32_t nic_ports_iters[APP_MAX_NIC_TX_PORTS_PER_IO_LCORE];
	} tx;
};

struct app_lcore_params_worker;

struct resubmit_parm {
	uint32_t request_id;
	uint64_t igress_ts;
	struct app_lcore_params_worker *lp;
	char *value;
	int vsize;
};

struct app_lcore_params_worker {
	/* Rings */
	struct rte_ring *rings_in[APP_MAX_IO_LCORES];
	uint32_t n_rings_in;
	struct rte_ring *rings_out[APP_MAX_NIC_PORTS];

	/* LPM table */
	struct rte_lpm *lpm_table;
	uint32_t worker_id;
	uint32_t lcore_id;
	uint32_t highest_chosen_inst;
	uint16_t app_port;
	/* Internal buffers */
	struct app_mbuf_array mbuf_in;
	struct app_mbuf_array mbuf_out[APP_MAX_NIC_PORTS];
	uint8_t mbuf_out_flush[APP_MAX_NIC_PORTS];

	/* Stats */
	uint32_t rings_in_count[APP_MAX_IO_LCORES];
	uint32_t rings_in_iters[APP_MAX_IO_LCORES];
	uint32_t rings_out_count[APP_MAX_NIC_PORTS];
	uint32_t rings_out_count_drop[APP_MAX_NIC_PORTS];
	uint32_t rings_out_iters[APP_MAX_NIC_PORTS];
	uint64_t nb_delivery;
	uint64_t nb_latency;
	uint64_t latency;
	uint64_t total_pkts;
	uint64_t total_bytes;
	uint64_t accepted_count;
	uint64_t start_ts;
    FILE *latency_fp;
	uint32_t buffer_count;
    char file_buffer[CHUNK_SIZE + 64];
	/* Libpaxos */
	struct learner *learner;
	struct acceptor *acceptor;
	deliver_cb deliver;
	void*	deliver_arg;
	worker_cb process_pkt;
	recv_cb app_recvfrom;
	uint32_t cur_inst;
	uint32_t has_holes;
	uint32_t artificial_drop;
	uint32_t max_inst;
	uint64_t last_timestamp;
	struct rte_timer stat_timer;
	struct rte_timer deliver_timer;
	struct rte_timer check_hole_timer;
	struct rte_timer recv_timer;
	struct rte_timer checkpoint_timer;
	struct rte_timer request_timer[MAX_N_CONCURRENT_REQUEST];
	char* default_value;
	uint32_t default_value_len;

	struct proposer *proposer;
	struct rte_timer preexecute_timer;
	struct rte_timer prepare_timer;
	struct rte_timer accept_timer;
	uint32_t request_id;
	struct resubmit_parm *resubmit_params[MAX_N_CONCURRENT_REQUEST];
};

struct app_lcore_params {
	union {
		struct app_lcore_params_io io;
		struct app_lcore_params_worker worker;
	};
	enum app_lcore_type type;
	struct rte_mempool *pool;
} __rte_cache_aligned;

struct app_lpm_rule {
	uint32_t ip;
	uint8_t depth;
	uint8_t if_out;
};

struct app_params {
	/* lcore */
	struct app_lcore_params lcore_params[APP_MAX_LCORES];

	/* NIC */
	uint8_t nic_rx_queue_mask[APP_MAX_NIC_PORTS][APP_MAX_RX_QUEUES_PER_NIC_PORT];
	uint8_t nic_tx_port_mask[APP_MAX_NIC_PORTS];

	/* mbuf pools */
	struct rte_mempool *pools[APP_MAX_SOCKETS];

	/* LPM tables */
	struct rte_lpm *lpm_tables[APP_MAX_SOCKETS];
	struct app_lpm_rule lpm_rules[APP_MAX_LPM_RULES];
	uint32_t n_lpm_rules;

	/* rings */
	uint32_t nic_rx_ring_size;
	uint32_t nic_tx_ring_size;
	uint32_t ring_rx_size;
	uint32_t ring_tx_size;

	/* burst size */
	uint32_t burst_size_io_rx_read;
	uint32_t burst_size_io_rx_write;
	uint32_t burst_size_io_tx_read;
	uint32_t burst_size_io_tx_write;
	uint32_t burst_size_worker_read;
	uint32_t burst_size_worker_write;

	/* load balancing */
	uint8_t pos_lb;

	/* Paxos configuration */
	struct p4xos_configuration p4xos_conf;
	uint64_t hz;

	uint8_t force_quit;

} __rte_cache_aligned;

extern struct app_params app;

#define RTE_LOGTYPE_P4XOS	RTE_LOGTYPE_USER1

int app_parse_args(int argc, char **argv);
void app_print_usage(void);
void app_init(void);
int app_lcore_main_loop(void *arg);

int app_get_nic_rx_queues_per_port(uint16_t port);
int app_get_lcore_for_nic_rx(uint16_t port, uint8_t queue,
			      uint32_t *lcore_out);
int app_get_lcore_for_nic_tx(uint16_t port, uint32_t *lcore_out);
int app_is_socket_used(uint32_t socket);
uint32_t app_get_lcores_io_rx(void);
uint32_t app_get_lcores_worker(void);
int app_get_lcore_worker(uint32_t worker_id);
struct app_lcore_params_worker* app_get_worker(uint32_t worker_id);
struct app_lcore_params_io* app_get_io(uint32_t lcore);
void app_print_params(void);
void submit(uint8_t worker_id, char* value, int size);
void submit_bulk(uint8_t worker_id, uint32_t nb_pkts,
    struct app_lcore_params_worker *lp, char *value, int size);
void submit_bulk_priority(uint8_t worker_id, uint32_t nb_pkts, char *value,
	int size);
void flush_port(uint16_t port);
void app_set_deliver_callback(deliver_cb, void *arg);
void app_set_worker_callback(worker_cb);
int learner_handler(struct rte_mbuf *pkt_in, void *arg);
int proposer_handler(struct rte_mbuf *pkt_in, void *arg);
int acceptor_handler(struct rte_mbuf *pkt_in, void *arg);
int leader_handler(struct rte_mbuf *pkt_in, void *arg);
int replica_handler(struct rte_mbuf *pkt_in, void *arg);
void app_set_stat_callback(rte_timer_cb_t, void *arg);
void app_init_learner(void);
void app_init_acceptor(void);
void app_init_leader(void);
void app_init_proposer(void);
void app_set_default_value(char *arg, uint32_t vlen);
void learner_call_deliver(struct rte_timer *timer, void *arg);
void learner_check_holes_cb(struct rte_timer *timer, void *arg);
void learner_check_holes(struct app_lcore_params_worker *lp);
void proposer_resubmit(struct rte_timer *timer, void *arg);
void reset_leader_instance(uint32_t worker_id);
double bytes_to_gbits(uint64_t bytes);
double cycles_to_ns(uint64_t cycles, uint64_t hz);
void send_prepare(struct app_lcore_params_worker *lp, uint32_t inst,
					uint32_t prepare_size, char* value, int size);
void fill_holes(struct app_lcore_params_worker *lp, uint32_t inst,
					uint32_t prepare_size, char* value, int size);
void send_accept(struct app_lcore_params_worker *lp, paxos_accept* accept);
void send_checkpoint_message(uint8_t worker_id, uint32_t inst);
int app_send_burst(uint16_t port, struct rte_mbuf **pkts, uint32_t n_pkts);
void timer_send_checkpoint(struct rte_timer *timer, void *arg);
struct rte_sched_port *app_init_sched_port(uint32_t portid,
                                                  uint32_t socketid);
void app_free_proposer(void);
void app_set_register_cb(uint16_t port, recv_cb cb);
void prepare_paxos_message(struct rte_mbuf *created_pkt, uint16_t port,
                        struct sockaddr_in* src, struct sockaddr_in* dst,
                        uint8_t msgtype, uint32_t inst, uint16_t rnd,
                        uint8_t worker_id, uint16_t node_id, uint32_t request_id,
						uint64_t igress_ts, char* value, int size);
int net_sendto(uint8_t worker_id, char* buf, size_t len, struct sockaddr_in *to);
void proposer_preexecute(struct app_lcore_params_worker *lp);
void pre_execute_prepare(__rte_unused struct rte_timer *timer, void *arg);
void send_to_acceptor(struct app_lcore_params_worker *lp, struct paxos_message *pm);
void paxos_stats(struct rte_mbuf *pkt_in, struct app_lcore_params_worker *lp);
void free_resubmit_params(struct resubmit_parm *parm);

#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif /* _MAIN_H_ */
