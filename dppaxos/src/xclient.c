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
#include <math.h>

#include "dpp_paxos.h"
#include "main.h"
#include "datastore.h"
#include "net_util.h"

#define RTE_LOGTYPE_XCLIENT RTE_LOGTYPE_USER1

#define RX_RING_SIZE 8192
#define TX_RING_SIZE 2048

#define NUM_MBUFS (8192 * 4)
#define MBUF_CACHE_SIZE 256

#define CHUNK_SIZE 4096
#define LEARNER_RECOVERY_PORT 39012

#define RATE_LIMITER

struct client_param {
  char file_buffer[CHUNK_SIZE + 64];
  FILE *stat_fp;
  int buffer_count;
  uint64_t delivered_count;
  uint64_t Bps;
  uint64_t pps;
  uint64_t start_ts;
  struct rte_timer stat_timer;
  struct rte_timer recv_timer;
  struct rte_mempool *mbuf_pool;
  struct rte_mbuf *base_pkt;
  uint32_t latency_pkts;
  uint64_t latencies;
  uint32_t learner_ip;
  uint32_t tx_iter_count;
  struct rte_sched_port *sched_port;
};

struct client_param client;


static void init_client() {
  uint32_t global_log_level = rte_log_get_global_level();
  rte_log_set_level(RTE_LOGTYPE_XCLIENT, global_log_level);
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
}

static void set_request(struct request *ap, uint32_t inst, uint8_t msg_type,
                        uint8_t key, uint16_t value) {
  ap->type = msg_type;
  ap->req.write.key = key;
  if (ap->type == WRITE_REQ) {
    ap->req.write.value = value;
  }
}

static inline void print_ts(struct rte_mbuf *base_pkt) {
  size_t paxos_offset = get_paxos_offset();
  struct paxos_hdr *paxos_hdr =
      rte_pktmbuf_mtod_offset(base_pkt, struct paxos_hdr *, paxos_offset);
  printf("Igress ts %20" PRIu64 "\n", rte_be_to_cpu_64(paxos_hdr->igress_ts));
}

static inline void reset_ts(struct rte_mbuf *base_pkt) {
  size_t paxos_offset = get_paxos_offset();
  struct paxos_hdr *paxos_hdr =
      rte_pktmbuf_mtod_offset(base_pkt, struct paxos_hdr *, paxos_offset);
  paxos_hdr->igress_ts = 0;
}

static inline void update_ts(struct rte_mbuf *base_pkt) {
  size_t paxos_offset = get_paxos_offset();
  struct paxos_hdr *paxos_hdr =
      rte_pktmbuf_mtod_offset(base_pkt, struct paxos_hdr *, paxos_offset);
  uint64_t now = rte_get_timer_cycles();
  paxos_hdr->igress_ts = rte_cpu_to_be_64(now);
}

static struct rte_mbuf *prepare_base_pkt(struct rte_mempool *mbuf_pool,
                                         uint16_t port, uint8_t worker_id,
                                         uint8_t get_igress) {

  struct rte_mbuf *base_pkt = rte_pktmbuf_alloc(mbuf_pool);
  if (base_pkt == NULL) {
    RTE_LOG(WARNING, XCLIENT,
            "Not enough entries in the mempools for new command\n");
    return NULL;
  }

  struct request ap;
  uint8_t msg_type = WRITE_REQ;
  uint16_t value = 2345;
  set_request(&ap, 0, msg_type, 1, value);

  prepare_message(base_pkt, port, app.p4xos_conf.src_addr,
                  app.p4xos_conf.dst_addr, app.p4xos_conf.msgtype, 0, 0,
                  worker_id, app.p4xos_conf.node_id, (char *)&ap,
                  sizeof(struct request));

  if (get_igress) {
    update_ts(base_pkt);
  }

  return base_pkt;
}

static int submit_requests(struct rte_mbuf *pkt) {
  uint16_t port = app.p4xos_conf.tx_port;
  prepare_message(pkt, port, app.p4xos_conf.src_addr,
                  app.p4xos_conf.dst_addr, app.p4xos_conf.msgtype, 0, 0,
                  0, app.p4xos_conf.node_id, NULL, 0);

  client.tx_iter_count++;

  struct rte_sched_queue_stats stats;
  uint16_t qlen;
  rte_sched_queue_read_stats(client.sched_port, 0, &stats, &qlen);
  uint32_t i, nb_eq = 0, nb_deq = 0, nb_tx = 0;
  nb_eq = SCHED_PORT_QUEUE_SIZE - qlen;
  if (unlikely(nb_eq > 0)) {
    struct rte_mbuf *prepare_pkts[nb_eq];
    uint32_t i;
    for (i = 0; i < nb_eq; i++) {
      prepare_pkts[i] = pkt;
      reset_ts(prepare_pkts[i]);
    }
    nb_eq = rte_sched_port_enqueue(client.sched_port, prepare_pkts, nb_eq);
  }

  struct rte_mbuf *pkts_tx[app.p4xos_conf.osd];
  nb_deq =
      rte_sched_port_dequeue(client.sched_port, pkts_tx, app.p4xos_conf.osd);
  if (likely(nb_deq == 0)) {
    return 0;
  }
  for (i = 0; i < nb_deq; i++)
    update_ts(pkts_tx[i]);
  /* Send burst of TX packets, to port X. */
  nb_tx = rte_eth_tx_burst(port, 0, pkts_tx, nb_deq);

  // printf("Loop[%u]: deq %u, tx %u\n", client.tx_iter_count, nb_deq, nb_tx);
  /* Free any unsent packets. */
  if (unlikely(nb_tx < nb_deq)) {
    for (i = nb_tx; i < nb_deq; i++)
      rte_pktmbuf_free(pkts_tx[i]);
  }
  return nb_tx;
}

static inline size_t
get_file_size(FILE *fp)
{
    fseek(fp, 0L, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    return sz;
}


static void
copy_buffer_to_pkt(struct rte_mbuf *pkt, uint16_t port, char* buffer, uint32_t buffer_size)
{
    struct ether_hdr *eth = rte_pktmbuf_mtod_offset(pkt, struct ether_hdr *, 0);
    set_ether_hdr(eth, ETHER_TYPE_IPv4, &mac1_addr, &mac2_addr);
    size_t ip_offset = sizeof(struct ether_hdr);
    struct ipv4_hdr *ip = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *, ip_offset);
    set_ipv4_hdr(ip, IPPROTO_UDP, app.p4xos_conf.src_addr, app.p4xos_conf.dst_addr);
    size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
    struct udp_hdr *udp = rte_pktmbuf_mtod_offset(pkt, struct udp_hdr *, udp_offset);
    size_t payload_offset = udp_offset + sizeof(struct udp_hdr);
    struct request *req = rte_pktmbuf_mtod_offset(pkt, struct request*, payload_offset);
    // MSGTYPE
    req->type = BACKUP_RES;
    // Partition
    req->req.backup_res.pid = 0;
    req->req.backup_res.bufsize = rte_cpu_to_be_32(buffer_size);
    rte_memcpy(req->req.backup_res.buffer, buffer, buffer_size);
    size_t dgram_len =  sizeof(struct udp_hdr) +
                        sizeof(req->type) +
                        sizeof(req->req.backup_res.pid) +
                        sizeof(req->req.backup_res.bufsize) +
                        buffer_size;
    printf("Buffer size %u Dgram len %zu\n", buffer_size, dgram_len);
    set_udp_hdr(udp, 12345, LEARNER_RECOVERY_PORT, dgram_len);
    size_t pkt_size = udp_offset + dgram_len;
    udp->dgram_len = rte_cpu_to_be_16(dgram_len);
    pkt->data_len = pkt_size;
    pkt->pkt_len = pkt_size;
    pkt->l2_len = sizeof(struct ether_hdr);
    pkt->l3_len = sizeof(struct ipv4_hdr);
    pkt->l4_len = dgram_len;
    pkt->ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
    udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, pkt->ol_flags);
}


static int send_file(char* filename)
{
    #define MAXBUFLEN 1024
    uint16_t port = app.p4xos_conf.tx_port;

    char source[MAXBUFLEN];
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        RTE_LOG(WARNING, XCLIENT, "Open file %s has errors\n", filename);
        return -1;
    }
    size_t fsize = get_file_size(fp);
    uint16_t n_pkts = ceil((double)fsize / MAXBUFLEN);
    struct rte_mbuf *pkts[n_pkts];
    if (rte_pktmbuf_alloc_bulk(client.mbuf_pool, pkts, n_pkts) < 0)
    {
        RTE_LOG(WARNING, XCLIENT, "Not enough entries in the mempools\n");
        return -1;
    }

    uint32_t i=0;
    size_t newLen = fread(source, sizeof(char), MAXBUFLEN, fp);
    while (newLen > 0)
    {
        copy_buffer_to_pkt(pkts[i], port, source, newLen);
        i++;
        newLen = fread(source, sizeof(char), MAXBUFLEN, fp);
        if ( ferror( fp ) != 0 ) {
            fputs("Error reading file", stderr);
            break;
        }
    }

    /* Send burst of TX packets, to port X. */
    printf("Send file %s in %u packets\n", filename, n_pkts);
    uint16_t sent;
    do {
        sent = rte_eth_tx_burst(port, 0, pkts, n_pkts);
        n_pkts -= sent;
    } while(n_pkts);
    fclose(fp);
    return 0;
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

  // size_t data_size = sizeof(struct paxos_hdr);
  // prepare_hw_checksum(pkt_in, data_size);
  uint16_t msgtype = paxos_hdr->msgtype;
  uint32_t inst = rte_be_to_cpu_32(paxos_hdr->inst);

  RTE_LOG(DEBUG, XCLIENT, "in PORT %u, msgtype %u, worker_id %u, instance %u\n",
          pkt_in->port, msgtype, paxos_hdr->worker_id, inst);

  switch (msgtype) {
  case PAXOS_CHOSEN: {
    uint64_t previous = rte_be_to_cpu_64(paxos_hdr->igress_ts);
    if (likely(previous > client.start_ts)) {
      uint64_t now = rte_get_timer_cycles();

      uint64_t latency = now - previous;
      if (cycles_to_ns(latency, app.hz) > 1000000) {
        RTE_LOG(DEBUG, XCLIENT,
                "HIGH Latency %u %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", inst,
                latency, now, previous);
      }
      client.latencies += latency;
      client.latency_pkts++;
      client.buffer_count += sprintf(&client.file_buffer[client.buffer_count],
                                     "%" PRIu64 "\n", latency);
      if (client.buffer_count >= CHUNK_SIZE) {
        fwrite(client.file_buffer, client.buffer_count, 1, client.stat_fp);
        client.buffer_count = 0;
      }
    } else if (previous > 0) {
      RTE_LOG(WARNING, XCLIENT, "Invalid Latency %" PRIu64 " %" PRIu64 "\n",
              previous, client.start_ts);
      struct in_addr src_addr;
      struct in_addr dst_addr;
      src_addr.s_addr = ip_hdr->src_addr;
      dst_addr.s_addr = ip_hdr->dst_addr;
      char str_ip_src[INET_ADDRSTRLEN], str_ip_dst[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &src_addr, str_ip_src, sizeof(str_ip_src));
      inet_ntop(AF_INET, &dst_addr, str_ip_dst, sizeof(str_ip_dst));
      RTE_LOG(WARNING, XCLIENT,
              "in PORT %u, [%s -> %s] msgtype %u, worker_id %u, instance %u\n",
              pkt_in->port, str_ip_src, str_ip_dst, msgtype,
              paxos_hdr->worker_id, inst);
    }
    client.delivered_count++;
    client.pps++;
    client.Bps += pkt_in->pkt_len + 8 + 4 + 12;
    return -5;
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
  double avg_latency = 0.0;
  if (client.latency_pkts > 0) {
    avg_latency = cycles_to_ns(client.latencies / client.latency_pkts, app.hz);
  }
  printf("%-8s\t%-4u\t%-4u\t%-8u\t%-8.1f\t%-10" PRIu64 "\t%-3.3f\n", "Stat",
         app.p4xos_conf.osd, app.p4xos_conf.ts_interval, client.latency_pkts,
         avg_latency, client.pps, bytes_to_gbits(client.Bps));
  client.latency_pkts = 0;
  client.latencies = 0;
  client.pps = 0;
  client.Bps = 0;
  struct rte_eth_stats stats;
  unsigned lcore = rte_lcore_id();
  uint16_t port = app.p4xos_conf.tx_port;
  rte_eth_stats_get(port, &stats);

  printf("I/O RX %u in (NIC port %u): NIC rx %" PRIu64 ", tx %" PRIu64
         ", missed %" PRIu64 ", rx err: %" PRIu64 ", tx err %" PRIu64
         ", mbuf err: %" PRIu64 "\n",
         lcore, port, stats.ipackets, stats.opackets, stats.imissed,
         stats.ierrors, stats.oerrors, stats.rx_nombuf);
}

static void __rte_unused submit_new_requests(__rte_unused struct rte_timer *timer,
                                __rte_unused void *arg) {

  RTE_LOG(WARNING, XCLIENT, "Resubmit new packets\n");
  submit_requests(client.base_pkt);
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

  if (port >= rte_eth_dev_count_avail())
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

static int lcore_rx(void *arg) {
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

  printf("\nCore %u Receiving packets. [Ctrl+C to quit]\n", rte_lcore_id());
  send_file("/home/danghu/backup/private/1/000003.log");
  struct rte_mbuf *base_pkt =
      prepare_base_pkt(client.mbuf_pool, port, app.p4xos_conf.node_id, 0);
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
    struct rte_mbuf *pkts_rx[RX_BURST];
    const uint32_t nb_rx = rte_eth_rx_burst(port, 0, pkts_rx, RX_BURST);

    if (likely(nb_rx > 0)) {
        uint32_t nb_tx = packet_handler(port, pkts_rx, nb_rx);
        /* Drop received packets */
        uint32_t k;
        for (k=0; k < nb_tx; k++)
            rte_pktmbuf_free(pkts_rx[k]);
    }
    submit_requests(base_pkt);
  }
  return 0;
}

static int master_core(void *arg) {
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

  printf("\nCore %u monitoring. [Ctrl+C to quit]\n", rte_lcore_id());

  /* Run until the application is quit or killed. */
  while (likely(client.delivered_count < app.p4xos_conf.max_inst)) {
    cur_tsc = rte_get_timer_cycles();
    diff_tsc = cur_tsc - prev_tsc;
    if (diff_tsc > TIMER_RESOLUTION_CYCLES) {
      rte_timer_manage();
      prev_tsc = cur_tsc;
    }
  }
  return 0;
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

  unsigned lcore_id = rte_get_master_lcore();

  rte_timer_init(&client.stat_timer);
  ret = rte_timer_reset(&client.stat_timer, app.hz, PERIODICAL, lcore_id,
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

  ret = rte_timer_reset(&client.recv_timer, app.hz * 3, SINGLE, lcore_id,
                        submit_new_requests, client.mbuf_pool);
  if (ret < 0) {
    printf("receiver timer is in the RUNNING state\n");
  }
#endif


  lcore_id = rte_get_next_lcore(lcore_id, 1, 1);
  rte_eal_remote_launch(lcore_rx, (void *)client.mbuf_pool, lcore_id);

  master_core(NULL);
  destroy_client();

  return 0;
}
