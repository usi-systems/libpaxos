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
#define APP_DEFAULT_MEMPOOL_BUFFERS   8192 * 4 * 2
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
#define APP_DEFAULT_NIC_RX_WTHRESH  0
#endif

#ifndef APP_DEFAULT_NIC_RX_FREE_THRESH
#define APP_DEFAULT_NIC_RX_FREE_THRESH  64
#endif

#ifndef APP_DEFAULT_NIC_RX_DROP_EN
#define APP_DEFAULT_NIC_RX_DROP_EN 0
#endif

/* NIC TX */
#ifndef APP_DEFAULT_NIC_TX_RING_SIZE
#define APP_DEFAULT_NIC_TX_RING_SIZE 4096
#endif

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#ifndef APP_DEFAULT_NIC_TX_PTHRESH
#define APP_DEFAULT_NIC_TX_PTHRESH  32
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
#define APP_DEFAULT_RING_RX_SIZE 8192 * 4
#endif

#ifndef APP_DEFAULT_RING_TX_SIZE
#define APP_DEFAULT_RING_TX_SIZE 8192 * 4
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

/* Paxos logic */
#ifndef APP_DEFAULT_NUM_ACCEPTORS
#define APP_DEFAULT_NUM_ACCEPTORS 1
#endif
#if (APP_DEFAULT_NUM_ACCEPTORS >= 7)
#error "APP_DEFAULT_NUM_ACCEPTORS is too big"
#endif

#define P4XOS_PORT 0x2379

#define APP_DEFAULT_IP_SRC_ADDR "192.168.4.95"
#define APP_DEFAULT_IP_DST_ADDR "192.168.4.98"
#define APP_DEFAULT_MESSAGE_TYPE 0x0003
#define APP_DEFAULT_BASELINE 0
#define APP_DEFAULT_MULTIPLE_DBS 0
#define APP_DEFAULT_RESET_INST 0
#define APP_DEFAULT_INCREASE_INST 0
#define APP_DEFAULT_RUN_PREPARE 0
#define APP_DEFAULT_TX_PORT 0
#define APP_DEFAULT_NODE_ID 0
#define APP_DEFAULT_CHECKPOINT_INTERVAL 0
#define APP_DEFAULT_SEND_RESPONSE 0
#define APP_DEFAULT_TS_INTERVAL 4
#define APP_DEFAULT_DROP 0
#define APP_DEFAULT_OUTSTANDING	8
#define APP_DEFAULT_MAX_INST	24000000
#define APP_DEFAULT_SENDING_RATE 10000

#define MAX_SCHED_SUBPORTS 1
#define MAX_SCHED_PIPES 1
#undef RTE_SCHED_PIPE_PROFILES_PER_PORT
#define RTE_SCHED_PIPE_PROFILES_PER_PORT 1
#define SCHED_PORT_QUEUE_SIZE 64

#define RESUBMIT

#define TIMER_RESOLUTION_CYCLES 20000000ULL /* around 10ms at 2 Ghz */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*deliver_cb)(unsigned int, unsigned int, char* value, size_t size, void* arg);
typedef int (*worker_cb)(struct rte_mbuf *pkt_in, void *arg);

struct p4xos_configuration {
	uint8_t num_acceptors;
	uint8_t multi_dbs;
	uint8_t msgtype;
	uint16_t tx_port;
	uint16_t node_id;
	uint32_t src_addr;
	uint32_t dst_addr;
	uint32_t osd;
	uint32_t max_inst;
	uint8_t inc_inst;
	uint8_t reset_inst;
	uint8_t baseline;
	uint8_t drop;
	uint8_t run_prepare;
	uint8_t respond_to_client;
	uint32_t checkpoint_interval;
	uint32_t ts_interval;
	uint32_t rate;
};

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

struct app_lcore_params_worker {
	/* Rings */
	struct rte_ring *rings_in[APP_MAX_IO_LCORES];
	uint32_t n_rings_in;
	struct rte_ring *rings_out[APP_MAX_NIC_PORTS];

	/* LPM table */
	struct rte_lpm *lpm_table;
	uint32_t worker_id;
	uint32_t lcore_id;

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

	/* Libpaxos */
	struct learner *learner;
	struct acceptor *acceptor;
	deliver_cb deliver;
	void*	deliver_arg;
	worker_cb process_pkt;
	uint64_t nb_delivery;
	uint64_t nb_latency;
	uint64_t latency;
	uint64_t total_pkts;
	uint64_t total_bytes;
	uint64_t accepted_count;
	uint32_t cur_inst;
	uint32_t has_holes;
	uint32_t artificial_drop;
	uint32_t max_inst;
	struct rte_timer stat_timer;
	struct rte_timer deliver_timer;
	struct rte_timer check_hole_timer;
	struct rte_timer recv_timer[APP_MAX_LCORES];
	char* default_value;
	uint32_t default_value_len;
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
void submit_bulk_priority(uint8_t worker_id, uint32_t nb_pkts, char *value, int size);
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
void learner_check_holes(struct rte_timer *timer, void *arg);
void proposer_resubmit(struct rte_timer *timer, void *arg);
void reset_leader_instance(uint32_t worker_id);
double bytes_to_gbits(uint64_t bytes);
void send_prepare(struct app_lcore_params_worker *lp, uint32_t inst, uint32_t prepare_size, char* value, int size);
void fill_holes(struct app_lcore_params_worker *lp, uint32_t inst, uint32_t prepare_size, char* value, int size);
void send_accept(struct app_lcore_params_worker *lp, paxos_accept* accept);
void prepare_message(struct rte_mbuf *created_pkt, uint16_t port, uint32_t src_addr,
						uint32_t dst_addr, uint8_t msgtype, uint32_t inst,
						uint16_t rnd, uint8_t worker_id, uint16_t node_id, char* value, int size);
void send_checkpoint_message(uint8_t worker_id, uint32_t inst);
void app_send_burst(uint16_t port, struct rte_mbuf **pkts, uint32_t n_pkts);
void timer_send_checkpoint(struct rte_timer *timer, void *arg);
struct rte_sched_port *app_init_sched_port(uint32_t portid,
                                                  uint32_t socketid);

#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif /* _MAIN_H_ */
