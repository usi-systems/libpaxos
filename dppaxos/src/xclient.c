/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <arpa/inet.h>
#include <inttypes.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_hash.h>
#include <rte_hexdump.h>
#include <rte_ip.h>
#include <rte_jhash.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_sched.h>
#include <rte_timer.h>
#include <rte_udp.h>
#include <stdint.h>

#include "app_hdr.h"
#include "dpp_paxos.h"
#include "main.h"

#define RTE_LOGTYPE_XCLIENT RTE_LOGTYPE_USER1

#define CLIENT_TIMEOUT
#define RATE_LIMITER
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 4096

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250

#define CHUNK_SIZE 4096

#define RES_MAP_ENTRIES 409600

#define MAX_SCHED_SUBPORTS 1
#define MAX_SCHED_PIPES 4096

int app_pipe_to_profile[MAX_SCHED_SUBPORTS][MAX_SCHED_PIPES];

struct client_param {
  char file_buffer[CHUNK_SIZE + 64];
  FILE *stat_fp;
  int buffer_count;
  uint64_t delivered_count;
  uint64_t Bps;
  uint64_t pps;
  struct rte_timer stat_timer;
  struct rte_timer recv_timer;
  struct rte_mempool *mbuf_pool;
  uint64_t latency_pkts;
  uint64_t latencies;
  uint32_t learner_ip;
  struct rte_hash *res_map;
  struct rte_sched_port *sched_port;
};

struct client_param client;

struct res_key {
  uint32_t inst;
  uint8_t worker_id;
};

static void __rte_unused init_res_map() {
  struct rte_hash_parameters params;
  params.entries = RES_MAP_ENTRIES;
  params.key_len = sizeof(struct res_key);
  params.hash_func = rte_jhash;
  params.hash_func_init_val = 0;
  params.socket_id = rte_socket_id();
  params.name = "res_map";
  client.res_map = rte_hash_create(&params);
  if (client.res_map == NULL) {
    rte_panic("Failed to create response map, errno = %d\n", rte_errno);
  }
}

static struct rte_sched_subport_params subport_params[MAX_SCHED_SUBPORTS] = {
    {
        .tb_rate = 1250000000,
        .tb_size = 1000000,
        .tc_rate = {1250000000, 1250000000, 1250000000, 1250000000},
        .tc_period = 10,
    },
};

static struct rte_sched_pipe_params
    pipe_profiles[RTE_SCHED_PIPE_PROFILES_PER_PORT] = {
        {
            /* Profile #0 */
            .tb_rate = 305175,
            .tb_size = 1000000,
            .tc_rate = {305175, 305175, 305175, 305175},
            .tc_period = 40,
            .wrr_weights = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        },
};

struct rte_sched_port_params port_params = {
    .name = "port_scheduler_0",
    .socket = 0, /* computed */
    .rate = 0,   /* computed */
    .mtu = 1500,
    .frame_overhead = RTE_SCHED_FRAME_OVERHEAD_DEFAULT,
    .n_subports_per_port = 1,
    .n_pipes_per_subport = 256,
    .qsize = {64, 64, 64, 64},
    .pipe_profiles = pipe_profiles,
    .n_pipe_profiles =
        sizeof(pipe_profiles) / sizeof(struct rte_sched_pipe_params),
};

static struct rte_sched_port *app_init_sched_port(uint32_t portid,
                                                  uint32_t socketid) {
  static char port_name[32]; /* static as referenced from global port_params*/
  struct rte_eth_link link;
  struct rte_sched_port *port = NULL;
  uint32_t pipe, subport;
  int err;
  rte_eth_link_get(portid, &link);
  if (link.link_status) {
    const char *dp = (link.link_duplex == ETH_LINK_FULL_DUPLEX) ? "full-duplex"
                                                                : "half-duplex";
    printf("\nPort %u Link Up - speed %u Mbps - %s\n", portid, link.link_speed,
           dp);
  }

  port_params.socket = socketid;
  port_params.rate = (uint64_t)link.link_speed * 1000 * 1000 / 8;
  snprintf(port_name, sizeof(port_name), "port_%d", portid);
  port_params.name = port_name;

  printf("RTE_SCHED_PIPE_PROFILES_PER_PORT %u\n",
         RTE_SCHED_PIPE_PROFILES_PER_PORT);
  printf("n_pipe_profiles %u\n", port_params.n_pipe_profiles);
  uint32_t i, j;
  for (i = 0; i < port_params.n_pipe_profiles; i++) {
    struct rte_sched_pipe_params *p = port_params.pipe_profiles + i;
    p->tb_rate = app.p4xos_conf.rate * 1000 * 1000 / 8;
    p->tb_size = 1000000;
    for (j = 0; j < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; j++) {
      p->tc_rate[j] = app.p4xos_conf.rate * 1000 * 1000 / 8;
    }
    p->tc_period = 10;
    for (j = 0; j < RTE_SCHED_QUEUES_PER_PIPE; j++) {
      p->wrr_weights[j] = 1;
    }
  }

  port = rte_sched_port_config(&port_params);
  if (port == NULL) {
    rte_exit(EXIT_FAILURE, "Unable to config sched port\n");
  }
  for (subport = 0; subport < port_params.n_subports_per_port; subport++) {
    err = rte_sched_subport_config(port, subport, &subport_params[subport]);
    if (err) {
      rte_exit(EXIT_FAILURE, "Unable to config sched subport %u, err=%d\n",
               subport, err);
    }
    for (pipe = 0; pipe < port_params.n_pipes_per_subport; pipe++) {
      if (app_pipe_to_profile[subport][pipe] != -1) {
        err = rte_sched_pipe_config(port, subport, pipe,
                                    app_pipe_to_profile[subport][pipe]);
        if (err) {
          rte_exit(EXIT_FAILURE, "Unable to config sched pipe %u "
                                 "for profile %d, err=%d\n",
                   pipe, app_pipe_to_profile[subport][pipe], err);
        }
      }
    }
  }
  return port;
}

static void init_client() {
  uint32_t global_log_level = rte_log_get_global_level();
  rte_log_set_level(RTE_LOGTYPE_XCLIENT, global_log_level);
#ifdef FILTER_RESPONSE
  init_res_map();
#endif
  client.stat_fp = fopen("latency.txt", "w");
  client.buffer_count = 0;
  client.delivered_count = 0;
}

static void destroy_client() {
  if (client.buffer_count > 0) {
    fwrite(client.file_buffer, 1, client.buffer_count, client.stat_fp);
  }
  fclose(client.stat_fp);
  rte_sched_port_free(client.sched_port);
  rte_hash_free(client.res_map);
}

static void set_app_hdr(struct app_hdr *ap, uint32_t inst, uint8_t msg_type,
                        uint8_t key, uint16_t value) {
  ap->msg_type = msg_type;
  ap->key = key;
  if (ap->msg_type == WRITE_OP) {
    ap->value = value;
  }
}

static void submit_requests(struct rte_mempool *mbuf_pool, uint32_t n_workers) {
  int ret;
  uint16_t port = app.p4xos_conf.tx_port;
  uint32_t n_reqs = app.p4xos_conf.osd;

  struct rte_mbuf *prepare_pkts[n_reqs];
  ret = rte_pktmbuf_alloc_bulk(mbuf_pool, prepare_pkts, n_reqs);

  if (ret < 0) {
    RTE_LOG(DEBUG, XCLIENT,
            "Not enough entries in the mempools for NEW_COMMAND\n");
    return;
  }

  uint32_t i;
  uint16_t value;

  uint8_t worker_id;
  struct app_hdr ap;
  for (i = 0; i < n_reqs; i++) {
    worker_id = i % n_workers;
    uint8_t msg_type = WRITE_OP;
    value = i * i;
    set_app_hdr(&ap, i, msg_type, i, value);

    prepare_message(prepare_pkts[i], port, app.p4xos_conf.src_addr,
                    app.p4xos_conf.dst_addr, app.p4xos_conf.msgtype, 0, 0,
                    worker_id, app.p4xos_conf.node_id, (char *)&ap,
                    sizeof(struct app_hdr));
  }

#ifdef RATE_LIMITER
  struct rte_mbuf *pkts_tx[n_reqs];

  uint32_t nb_eq =
      rte_sched_port_enqueue(client.sched_port, prepare_pkts, n_reqs);

  if (unlikely(nb_eq == 0))
    return;

  uint32_t n_pkts_tx =
      rte_sched_port_dequeue(client.sched_port, pkts_tx, nb_eq);
  /* Send burst of TX packets, to port X. */
  uint32_t nb_tx = rte_eth_tx_burst(port, 0, pkts_tx, n_pkts_tx);
  /* Free any unsent packets. */
  if (unlikely(nb_tx < n_pkts_tx)) {
    uint32_t idx;
    for (idx = nb_tx; idx < n_pkts_tx; idx++)
      rte_pktmbuf_free(pkts_tx[idx]);
  }
#else
  uint32_t idx;
  idx = rte_eth_tx_burst(port, 0, prepare_pkts, n_reqs);
  if (unlikely(idx < n_reqs)) {
    for (; idx < n_reqs; idx++)
      rte_pktmbuf_free(prepare_pkts[idx]);
  }
#endif
}

static void reset_instance(struct rte_mempool *mbuf_pool, uint32_t n_workers) {
  int ret;
  uint16_t port = app.p4xos_conf.tx_port;

  struct rte_mbuf *prepare_pkts[n_workers];
  ret = rte_pktmbuf_alloc_bulk(mbuf_pool, prepare_pkts, n_workers);

  if (ret < 0) {
    RTE_LOG(DEBUG, XCLIENT, "Not enough entries in the mempools for RESET\n");
    return;
  }

  uint32_t i;
  for (i = 0; i < n_workers; i++) {
    prepare_message(prepare_pkts[i], port, app.p4xos_conf.src_addr,
                    app.p4xos_conf.dst_addr, PAXOS_RESET, 0, 0, i,
                    app.p4xos_conf.node_id, NULL, 0);
  }

  uint32_t buf;
  buf = rte_eth_tx_burst(port, 0, prepare_pkts, n_workers);
  if (unlikely(buf < n_workers)) {
    for (; buf < n_workers; buf++)
      rte_pktmbuf_free(prepare_pkts[buf]);
  }
}

static const struct rte_eth_conf port_conf_default = {
    .rxmode =
        {
            .max_rx_pkt_len = ETHER_MAX_LEN, .ignore_offload_bitfield = 1,
        },
};

static void swap_ips(struct ipv4_hdr *ip) {
  uint32_t tmp = ip->dst_addr;
  ip->dst_addr = ip->src_addr;
  ip->src_addr = tmp;
}

static void set_ips(struct ipv4_hdr *ip) {
  ip->dst_addr = app.p4xos_conf.dst_addr;
  ip->src_addr = app.p4xos_conf.src_addr;
}

static int paxos_handler(uint16_t in_port, struct rte_mbuf *pkt_in) {
  int ret = filter_packets(pkt_in);
  if (ret < 0) {
    return ret;
  }
  struct ipv4_hdr *ip_hdr = rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *,
                                                    sizeof(struct ether_hdr));

  size_t paxos_offset = get_paxos_offset();
  struct paxos_hdr *paxos_hdr =
      rte_pktmbuf_mtod_offset(pkt_in, struct paxos_hdr *, paxos_offset);

  size_t data_size = sizeof(struct paxos_hdr);
  prepare_hw_checksum(pkt_in, data_size);
  uint16_t msgtype = paxos_hdr->msgtype;
  uint32_t inst = rte_be_to_cpu_32(paxos_hdr->inst);

  RTE_LOG(DEBUG, XCLIENT, "in PORT %u, msgtype %u, worker_id %u, instance %u\n",
          pkt_in->port, msgtype, paxos_hdr->worker_id, inst);

  switch (msgtype) {
  case PAXOS_CHOSEN: {
#ifdef FILTER_RESPONSE
    struct res_key key = {.inst = inst, .worker_id = paxos_hdr->worker_id};
    ret = rte_hash_lookup(client.res_map, (void *)&key);
    if (ret == -EINVAL) {
      RTE_LOG(DEBUG, XCLIENT,
              "LOOKUP Invalid key (worker %u, inst %u), ret %d\n",
              paxos_hdr->worker_id, inst, ret);
      return -1;
    } else if (ret != -ENOENT) {
      RTE_LOG(DEBUG, XCLIENT, "Key existed (worker %u, inst %u), ret %d\n",
              paxos_hdr->worker_id, inst, ret);
      return -1;
    }
    ret = rte_hash_add_key(client.res_map, (void *)&key);
    if (ret == -EINVAL) {
      RTE_LOG(DEBUG, XCLIENT,
              "ADD KEY ERROR: Invalid key (worker %u, inst %u), ret %d\n",
              paxos_hdr->worker_id, inst, ret);
      return -1;
    } else if (ret < 0) {
      RTE_LOG(DEBUG, XCLIENT, "Failed to add mapping for (worker "
                              "%u, inst %u), ret %d\n",
              paxos_hdr->worker_id, inst, ret);
      return -1;
    }

    RTE_LOG(DEBUG, XCLIENT, "Add Key (worker %u, inst %u), ret %d\n",
            paxos_hdr->worker_id, inst, ret);
#endif

    uint64_t previous = rte_be_to_cpu_64(paxos_hdr->igress_ts);
    if (unlikely(previous > 0)) {
      uint64_t now = rte_get_timer_cycles();
      uint64_t latency = now - previous;
      client.latencies += latency;
      client.latency_pkts++;
      client.buffer_count += sprintf(&client.file_buffer[client.buffer_count],
                                     "%" PRIu64 "\n", latency);
      if (client.buffer_count >= CHUNK_SIZE) {
        fwrite(client.file_buffer, client.buffer_count, 1, client.stat_fp);
        client.buffer_count = 0;
      }
      paxos_hdr->igress_ts = rte_cpu_to_be_64(now);
    } else if (unlikely(rte_be_to_cpu_32(paxos_hdr->inst) %
                            app.p4xos_conf.ts_interval ==
                        0)) {
      uint64_t now = rte_get_timer_cycles();
      paxos_hdr->igress_ts = rte_cpu_to_be_64(now);
    }

    set_ips(ip_hdr);
    paxos_hdr->msgtype = app.p4xos_conf.msgtype;
    client.delivered_count++;
    client.pps++;
    client.Bps += pkt_in->pkt_len;
    break;
  }
  default: {
    struct in_addr src_addr;
    struct in_addr dst_addr;
    src_addr.s_addr = ip_hdr->src_addr;
    dst_addr.s_addr = ip_hdr->dst_addr;
    char str_ip_src[INET_ADDRSTRLEN], str_ip_dst[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &src_addr, str_ip_src, sizeof(str_ip_src));
    inet_ntop(AF_INET, &dst_addr, str_ip_dst, sizeof(str_ip_dst));
    RTE_LOG(WARNING, XCLIENT, "No handler for %u\n", msgtype);
    RTE_LOG(WARNING, XCLIENT,
            "in PORT %u, [%s -> %s] msgtype %u, worker_id %u, instance %u\n",
            pkt_in->port, str_ip_src, str_ip_dst, msgtype, paxos_hdr->worker_id,
            inst);
    return -4;
  }
  }
  return 0;
}

static void stat_cb(__rte_unused struct rte_timer *timer,
                    __rte_unused void *arg) {
  if (client.latency_pkts > 0) {
    double avg_latency = (double)client.latencies / client.latency_pkts;
    printf("%-8s\t%-4u\t%-4u\t%-8.1f\t%-10" PRIu64 "\t%-3.3f\n", "Stat",
           app.p4xos_conf.osd, app.p4xos_conf.ts_interval, avg_latency,
           client.pps, bytes_to_gbits(client.Bps));
    client.latency_pkts = 0;
    client.latencies = 0;
    client.pps = 0;
    client.Bps = 0;
  }
}

static void submit_new_requests(__rte_unused struct rte_timer *timer,
                                __rte_unused void *arg) {

  struct rte_mempool *mbuf_pool = (struct rte_mempool *)arg;
  uint32_t n_workers = app_get_lcores_worker();
  RTE_LOG(WARNING, XCLIENT, "Resubmit new packets\n");
  submit_requests(mbuf_pool, n_workers);
  int ret =
      rte_timer_reset(&client.recv_timer, app.hz * 3, SINGLE, rte_lcore_id(),
                      submit_new_requests, client.mbuf_pool);
  if (ret < 0) {
    printf("receiver timer is in the RUNNING state\n");
  }
}

static uint16_t packet_handler(uint16_t in_port, struct rte_mbuf **pkts,
                               uint16_t n_pkts) {
  RTE_LOG(DEBUG, XCLIENT, "Proccessed %u packets\n", n_pkts);

  int ret;
  uint16_t buf;
  uint16_t tx_pkts = 0;
  for (buf = 0; buf < n_pkts; buf++) {
    ret = paxos_handler(in_port, pkts[buf]);
    if (ret < 0) {
      rte_pktmbuf_free(pkts[buf]);
      continue;
    }
    tx_pkts++;
  }
  RTE_LOG(DEBUG, XCLIENT, "Accepted %u packets\n", tx_pkts);

#ifdef CLIENT_TIMEOUT
  ret = rte_timer_reset(&client.recv_timer, app.hz * 3, SINGLE, rte_lcore_id(),
                        submit_new_requests, client.mbuf_pool);
  if (ret < 0) {
    printf("receiver timer is in the RUNNING state\n");
  }
#endif

  return tx_pkts;
}
/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int port_init(uint16_t port, struct rte_mempool *mbuf_pool) {
  struct rte_eth_conf port_conf = port_conf_default;
  const uint16_t rx_rings = 1, tx_rings = 1;
  uint16_t nb_rxd = RX_RING_SIZE;
  uint16_t nb_txd = TX_RING_SIZE;
  int retval;
  uint16_t q;
  struct rte_eth_dev_info dev_info;
  struct rte_eth_txconf txconf;

  if (port >= rte_eth_dev_count())
    return -1;

  rte_eth_dev_info_get(port, &dev_info);
  if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
    port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;

  /* Configure the Ethernet device. */
  retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
  if (retval != 0)
    return retval;

  retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
  if (retval != 0)
    return retval;

  /* Allocate and set up 1 RX queue per Ethernet port. */
  for (q = 0; q < rx_rings; q++) {
    retval = rte_eth_rx_queue_setup(
        port, q, nb_rxd, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
    if (retval < 0)
      return retval;
  }

  txconf = dev_info.default_txconf;
  txconf.txq_flags = ETH_TXQ_FLAGS_IGNORE;
  txconf.offloads = port_conf.txmode.offloads;
  /* Allocate and set up 1 TX queue per Ethernet port. */
  for (q = 0; q < tx_rings; q++) {
    retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                                    rte_eth_dev_socket_id(port), &txconf);
    if (retval < 0)
      return retval;
  }

  /* Start the Ethernet port. */
  retval = rte_eth_dev_start(port);
  if (retval < 0)
    return retval;

  /* Display the port MAC address. */
  struct ether_addr addr;
  rte_eth_macaddr_get(port, &addr);
  printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8
         " %02" PRIx8 " %02" PRIx8 "\n",
         port, addr.addr_bytes[0], addr.addr_bytes[1], addr.addr_bytes[2],
         addr.addr_bytes[3], addr.addr_bytes[4], addr.addr_bytes[5]);

  /* Enable RX in promiscuous mode for the Ethernet device. */
  rte_eth_promiscuous_enable(port);

  return 0;
}

static void lcore_main(struct rte_mempool *mbuf_pool) {
  uint16_t port = app.p4xos_conf.tx_port;
  uint64_t prev_tsc = 0, cur_tsc, diff_tsc;
  /*
   * Check that the port is on the same NUMA node as the polling thread
   * for best performance.
   */
  if (rte_eth_dev_socket_id(port) > 0 &&
      rte_eth_dev_socket_id(port) != (int)rte_socket_id())
    printf("WARNING, port %u is on remote NUMA node to "
           "polling thread.\n\tPerformance will "
           "not be optimal.\n",
           port);

  printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n", rte_lcore_id());

  uint32_t n_workers = app_get_lcores_worker();

  if (app.p4xos_conf.reset_inst)
    reset_instance(mbuf_pool, n_workers);

  submit_requests(mbuf_pool, n_workers);

  /* Run until the application is quit or killed. */
  while (client.delivered_count < app.p4xos_conf.max_inst) {
    cur_tsc = rte_get_timer_cycles();
    diff_tsc = cur_tsc - prev_tsc;
    if (diff_tsc > TIMER_RESOLUTION_CYCLES) {
      rte_timer_manage();
      prev_tsc = cur_tsc;
    }
    /* Get burst of RX packets, from first port. */
    uint32_t RX_BURST = app.burst_size_io_rx_read;
    uint32_t TX_BURST = app.burst_size_io_tx_write;
    struct rte_mbuf *pkts_rx[RX_BURST], *pkts_tx[TX_BURST];
    const uint32_t nb_rx = rte_eth_rx_burst(port, 0, pkts_rx, RX_BURST);

    if (unlikely(nb_rx == 0))
      continue;
    uint32_t nb_to_tx = packet_handler(port, pkts_rx, nb_rx);
    if (unlikely(nb_to_tx == 0))
      continue;

#ifdef RATE_LIMITER
    uint32_t nb_eq =
        rte_sched_port_enqueue(client.sched_port, pkts_rx, nb_to_tx);

    if (unlikely(nb_to_tx == 0))
      continue;
    uint32_t n_pkts_tx =
        rte_sched_port_dequeue(client.sched_port, pkts_tx, nb_eq);
    /* Send burst of TX packets, to port X. */
    uint32_t nb_tx = rte_eth_tx_burst(port, 0, pkts_tx, n_pkts_tx);
    if (unlikely(n_pkts_tx < nb_eq) | (unlikely(nb_tx < n_pkts_tx))) {
      RTE_LOG(WARNING, XCLIENT, "Rx %u, Accepted %u, Enq %u Deq %u Tx %u\n",
              nb_rx, nb_to_tx, nb_eq, n_pkts_tx, nb_tx);
    }
    /* Free any unsent packets. */
    if (unlikely(nb_tx < n_pkts_tx)) {
      uint32_t idx;
      for (idx = nb_tx; idx < n_pkts_tx; idx++)
        rte_pktmbuf_free(pkts_tx[idx]);
    }
#else
    uint32_t n_pkts_tx = nb_to_tx;
    /* Send burst of TX packets, to port X. */
    uint32_t nb_tx = rte_eth_tx_burst(port, 0, pkts_rx, n_pkts_tx);
    /* Free any unsent packets. */
    if (unlikely(nb_tx < n_pkts_tx)) {
      uint32_t idx;
      for (idx = nb_tx; idx < n_pkts_tx; idx++)
        rte_pktmbuf_free(pkts_rx[idx]);
    }
#endif
  }
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int main(int argc, char *argv[]) {

  /* Initialize the Environment Abstraction Layer (EAL). */
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

  argc -= ret;
  argv += ret;

  /* Parse application arguments (after the EAL ones) */
  ret = app_parse_args(argc, argv);
  if (ret < 0) {
    app_print_usage();
    return -1;
  }
  app_print_params();

  init_client();

  /* Stats */
  rte_timer_subsystem_init();
  /* fetch default timer frequency. */
  app.hz = rte_get_timer_hz();

  rte_timer_init(&client.stat_timer);
  ret = rte_timer_reset(&client.stat_timer, app.hz, PERIODICAL, rte_lcore_id(),
                        stat_cb, &client);
  if (ret < 0) {
    printf("timer is in the RUNNING state\n");
  }
  uint16_t portid = app.p4xos_conf.tx_port;
  /* Creates a new mempool in memory to hold the mbufs. */
  client.mbuf_pool =
      rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS, MBUF_CACHE_SIZE, 0,
                              RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

  if (client.mbuf_pool == NULL)
    rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

#ifdef RATE_LIMITER
  client.sched_port = app_init_sched_port(portid, rte_socket_id());
  if (client.sched_port == NULL)
    rte_exit(EXIT_FAILURE, "Cannot init scheduled port\n");
#endif

  /* Initialize port. */
  if (port_init(portid, client.mbuf_pool) != 0)
    rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n", portid);

#ifdef CLIENT_TIMEOUT
  rte_timer_init(&client.recv_timer);

  ret = rte_timer_reset(&client.recv_timer, app.hz * 3, SINGLE, rte_lcore_id(),
                        submit_new_requests, client.mbuf_pool);
  if (ret < 0) {
    printf("receiver timer is in the RUNNING state\n");
  }
#endif
  /* Call lcore_main on the master core only. */
  lcore_main(client.mbuf_pool);

  destroy_client();

  return 0;
}
