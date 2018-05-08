#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_hexdump.h>
#include <rte_interrupts.h>
#include <rte_ip.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_lpm.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_per_lcore.h>
#include <rte_prefetch.h>
#include <rte_random.h>
#include <rte_ring.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include "acceptor.h"
#include "dpp_paxos.h"
#include "learner.h"
#include "main.h"
#include "paxos.h"

#define PREAMBLE_CRC_IPG 24

/* Convert bytes to Gbit */
inline double bytes_to_gbits(uint64_t bytes) {
  double t = bytes;
  t *= (double)8;
  t /= 1000 * 1000 * 1000;
  return t;
}

static void set_ips(struct ipv4_hdr *ip) {
  ip->dst_addr = ip->src_addr;
  ip->src_addr = app.p4xos_conf.src_addr;
}

size_t get_paxos_offset(void) {
  return sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) +
         sizeof(struct udp_hdr);
}

void prepare_hw_checksum(struct rte_mbuf *pkt_in, size_t data_size) {
  size_t ip_offset = sizeof(struct ether_hdr);
  struct ipv4_hdr *ip =
      rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);
  size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
  struct udp_hdr *udp =
      rte_pktmbuf_mtod_offset(pkt_in, struct udp_hdr *, udp_offset);
  udp->dgram_len = rte_cpu_to_be_16(sizeof(struct udp_hdr) + data_size);
  pkt_in->l2_len = sizeof(struct ether_hdr);
  pkt_in->l3_len = sizeof(struct ipv4_hdr);
  pkt_in->l4_len = sizeof(struct udp_hdr) + data_size;
  size_t pkt_size = pkt_in->l2_len + pkt_in->l3_len + pkt_in->l4_len;
  pkt_in->data_len = pkt_size;
  pkt_in->pkt_len = pkt_size;
  pkt_in->ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
  udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, pkt_in->ol_flags);
}

int filter_packets(struct rte_mbuf *pkt_in) {
  struct ether_hdr *eth =
      rte_pktmbuf_mtod_offset(pkt_in, struct ether_hdr *, 0);
  if (rte_be_to_cpu_16(eth->ether_type) != ETHER_TYPE_IPv4)
    return NON_ETHERNET_PACKET;
  size_t ip_offset = sizeof(struct ether_hdr);
  struct ipv4_hdr *ip =
      rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);
  if (ip->next_proto_id != IPPROTO_UDP)
    return NON_UDP_PACKET;
  size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
  struct udp_hdr *udp =
      rte_pktmbuf_mtod_offset(pkt_in, struct udp_hdr *, udp_offset);
  if (rte_be_to_cpu_16(udp->dst_port) != P4XOS_PORT)
    return NON_PAXOS_PACKET;
  return SUCCESS;
}

static inline void respond(struct rte_mbuf *pkt_in) {
  size_t ip_offset = sizeof(struct ether_hdr);
  struct ipv4_hdr *ip =
      rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);
  set_ips(ip);
  size_t data_size = sizeof(struct paxos_hdr);
  prepare_hw_checksum(pkt_in, data_size);
}

static inline int prepare_handler(struct paxos_hdr *paxos_hdr,
                                  struct app_lcore_params_worker *lp) {
  struct paxos_prepare prepare = {
      .iid = rte_be_to_cpu_32(paxos_hdr->inst),
      .ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
  };
  paxos_message out;
  if (acceptor_receive_prepare(lp->acceptor, &prepare, &out) != 0) {
    paxos_hdr->msgtype = out.type;
    paxos_hdr->acptid = rte_cpu_to_be_16(app.p4xos_conf.node_id);
  } else {
    return NO_MAJORITY;
  }
  return SUCCESS;
}

static inline int accept_handler(struct paxos_hdr *paxos_hdr,
                                 struct app_lcore_params_worker *lp) {
  int vsize = 4; // rte_be_to_cpu_32(paxos_hdr->value_len);
  struct paxos_accept accept = {.iid = rte_be_to_cpu_32(paxos_hdr->inst),
                                .ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
                                .value_ballot =
                                    rte_be_to_cpu_16(paxos_hdr->vrnd),
                                .aid = rte_be_to_cpu_16(paxos_hdr->acptid),
                                .value = {vsize, (char *)&paxos_hdr->value}};
  paxos_message out;
  if (acceptor_receive_accept(lp->acceptor, &accept, &out) != 0) {
    paxos_hdr->msgtype = out.type;
    paxos_hdr->acptid = rte_cpu_to_be_16(app.p4xos_conf.node_id);
    lp->accepted_count++;
    RTE_LOG(DEBUG, P4XOS, "Worker %u Accepted instance %u\n", lp->worker_id,
            rte_be_to_cpu_32(paxos_hdr->inst));
  } else {
    return NO_MAJORITY;
  }
  return SUCCESS;
}

static inline int promise_handler(struct paxos_hdr *paxos_hdr,
                                  struct app_lcore_params_worker *lp) {
  int ret = 0;
  int vsize = 4; // rte_be_to_cpu_32(paxos_hdr->value_len);
  struct paxos_promise promise = {
      .iid = rte_be_to_cpu_32(paxos_hdr->inst),
      .ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
      .value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
      .aid = rte_be_to_cpu_16(paxos_hdr->acptid),
      .value = {vsize, (char *)&paxos_hdr->value},
  };
  RTE_LOG(DEBUG, P4XOS, "Worker %u Received Promise instance %u, ballot %u, "
                        "value_ballot %u, aid %u\n",
          lp->worker_id, rte_be_to_cpu_32(paxos_hdr->inst), promise.ballot,
          promise.value_ballot, promise.aid);
  paxos_message pa;
  ret = learner_receive_promise(lp->learner, &promise, &pa.u.accept);
  if (ret) {
    send_accept(lp, &pa.u.accept);
    return DROP_ORIGINAL_PACKET;
  } else {
    return NO_MAJORITY;
  }
}

static inline int accepted_handler(struct paxos_hdr *paxos_hdr,
                                   struct app_lcore_params_worker *lp) {
  int ret = 0;
  // Artificial DROP packet
  if (lp->artificial_drop) {
    if (rand() % 1299827 == 0)
      return DROP_ORIGINAL_PACKET;
  }
  int vsize = 4; // rte_be_to_cpu_32(paxos_hdr->value_len);
  struct paxos_accepted ack = {.iid = rte_be_to_cpu_32(paxos_hdr->inst),
                               .ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
                               .value_ballot =
                                   rte_be_to_cpu_16(paxos_hdr->vrnd),
                               .aid = rte_be_to_cpu_16(paxos_hdr->acptid),
                               .value = {vsize, (char *)&paxos_hdr->value}};
  RTE_LOG(DEBUG, P4XOS, "Worker %u, log_index %u, Received Accepted instance "
                        "%u, aid %u, ballot %u\n",
          lp->worker_id, rte_be_to_cpu_16(paxos_hdr->log_index), ack.iid,
          ack.aid, ack.ballot);
  learner_receive_accepted(lp->learner, &ack);
  paxos_accepted out;
  ret = learner_deliver_next(lp->learner, &out);
  if (ret) {
    lp->deliver(lp->worker_id, out.iid, out.value.paxos_value_val,
                out.value.paxos_value_len, lp->deliver_arg);
    paxos_hdr->msgtype = PAXOS_CHOSEN;
    paxos_accepted_destroy(&out);
  } else {
    return NO_MAJORITY;
  }
  return SUCCESS;
}

static inline int new_command_handler(struct paxos_hdr *paxos_hdr,
                                      struct app_lcore_params_worker *lp) {
  paxos_hdr->inst = rte_cpu_to_be_32(lp->cur_inst++);
  paxos_hdr->msgtype = PAXOS_ACCEPT;
  return SUCCESS;
}

static inline int chosen_handler(struct paxos_hdr *paxos_hdr,
                                 struct app_lcore_params_worker *lp) {
  uint64_t previous = rte_be_to_cpu_64(paxos_hdr->igress_ts);
  if (previous > 0) {
    uint64_t now = rte_get_timer_cycles();
    uint64_t latency = now - previous;
    lp->latency += latency;
    lp->nb_latency++;
    paxos_hdr->igress_ts = rte_cpu_to_be_64(now);
  }
  lp->nb_delivery++;
  paxos_hdr->msgtype = app.p4xos_conf.msgtype;
  return SUCCESS;
}

static inline void stats(struct rte_mbuf *pkt_in,
                         struct app_lcore_params_worker *lp) {
  lp->total_pkts++;
  lp->total_bytes += pkt_in->pkt_len + PREAMBLE_CRC_IPG;
}

int proposer_handler(struct rte_mbuf *pkt_in, void *arg) {
  struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
  stats(pkt_in, lp);
  int ret = filter_packets(pkt_in);
  if (ret < 0) {
    RTE_LOG(DEBUG, P4XOS, "Drop packets. Code %d\n", ret);
    return ret;
  }
  size_t paxos_offset = get_paxos_offset();
  struct paxos_hdr *paxos_hdr =
      rte_pktmbuf_mtod_offset(pkt_in, struct paxos_hdr *, paxos_offset);
  uint8_t msgtype = paxos_hdr->msgtype;

  switch (msgtype) {
  case PAXOS_CHOSEN: {
    chosen_handler(paxos_hdr, lp);
    break;
  }
  default:
    RTE_LOG(DEBUG, P4XOS, "No handler for %u\n", msgtype);
    return NO_HANDLER;
  }

  respond(pkt_in);
  return SUCCESS;
}

int leader_handler(struct rte_mbuf *pkt_in, void *arg) {
  struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
  stats(pkt_in, lp);
  int ret = filter_packets(pkt_in);
  if (ret < 0) {
    RTE_LOG(DEBUG, P4XOS, "Drop packets. Code %d\n", ret);
    return ret;
  }
  size_t paxos_offset = get_paxos_offset();
  struct paxos_hdr *paxos_hdr =
      rte_pktmbuf_mtod_offset(pkt_in, struct paxos_hdr *, paxos_offset);
  uint8_t msgtype = paxos_hdr->msgtype;

  switch (msgtype) {
  case NEW_COMMAND: {
    new_command_handler(paxos_hdr, lp);
    break;
  }
  default: {
    RTE_LOG(DEBUG, P4XOS, "No handler for %u\n", msgtype);
    return NO_HANDLER;
  }
  }

  respond(pkt_in);
  return SUCCESS;
}

int acceptor_handler(struct rte_mbuf *pkt_in, void *arg) {
  struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
  stats(pkt_in, lp);
  int ret = filter_packets(pkt_in);
  if (ret < 0) {
    RTE_LOG(DEBUG, P4XOS, "Drop packets. Code %d\n", ret);
    return ret;
  }
  size_t paxos_offset = get_paxos_offset();
  struct paxos_hdr *paxos_hdr =
      rte_pktmbuf_mtod_offset(pkt_in, struct paxos_hdr *, paxos_offset);
  uint8_t msgtype = paxos_hdr->msgtype;

  switch (msgtype) {
  case PAXOS_PREPARE: {
    prepare_handler(paxos_hdr, lp);
    break;
  }
  case PAXOS_ACCEPT: {
    accept_handler(paxos_hdr, lp);
    break;
  }
  default: {
    RTE_LOG(DEBUG, P4XOS, "No handler for %u\n", msgtype);
    return NO_HANDLER;
  }
  }

  respond(pkt_in);
  return SUCCESS;
}

int learner_handler(struct rte_mbuf *pkt_in, void *arg) {
  struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
  stats(pkt_in, lp);
  int ret = filter_packets(pkt_in);
  if (ret < 0) {
    RTE_LOG(DEBUG, P4XOS, "Drop packets. Code %d\n", ret);
    return ret;
  }
  size_t paxos_offset = get_paxos_offset();
  struct paxos_hdr *paxos_hdr =
      rte_pktmbuf_mtod_offset(pkt_in, struct paxos_hdr *, paxos_offset);
  uint8_t msgtype = paxos_hdr->msgtype;

  switch (msgtype) {
  case PAXOS_PROMISE: {
    prepare_handler(paxos_hdr, lp);
    break;
  }
  case PAXOS_ACCEPTED: {
    accepted_handler(paxos_hdr, lp);
    break;
  }
  default: {
    RTE_LOG(DEBUG, P4XOS, "No handler for %u\n", msgtype);
    return NO_HANDLER;
  }
  }

  respond(pkt_in);
  return SUCCESS;
}

void learner_call_deliver(__rte_unused struct rte_timer *timer,
                          __rte_unused void *arg) {
  struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
  paxos_accepted out;
  while (learner_deliver_next(lp->learner, &out)) {
    lp->deliver(lp->worker_id, out.iid, out.value.paxos_value_val,
                out.value.paxos_value_len, lp->deliver_arg);
    RTE_LOG(DEBUG, P4XOS, "Finished instance %u\n", out.iid);
    submit(lp->worker_id, out.value.paxos_value_val, out.value.paxos_value_len);
    paxos_accepted_destroy(&out);
  }
}

void learner_check_holes(__rte_unused struct rte_timer *timer,
                         __rte_unused void *arg) {
  struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
  if (lp->has_holes) {
    paxos_accepted out;
    while (learner_deliver_next(lp->learner, &out)) {
      lp->deliver(lp->worker_id, out.iid, out.value.paxos_value_val,
                  out.value.paxos_value_len, lp->deliver_arg);
      RTE_LOG(DEBUG, P4XOS, "%s Finished instance %u\n", __func__, out.iid);
      paxos_accepted_destroy(&out);
    }
    lp->has_holes = 0;
  }
  uint32_t from, to;
  if (learner_has_holes(lp->learner, &from, &to)) {
    lp->has_holes = 1;
    RTE_LOG(WARNING, P4XOS, "Learner %u Holes from %u to %u\n", lp->worker_id,
            from, to);
    uint32_t prepare_size = to - from;
    if (prepare_size > APP_DEFAULT_NIC_TX_PTHRESH) {
      prepare_size = APP_DEFAULT_NIC_TX_PTHRESH;
    }
    if (app.p4xos_conf.run_prepare) {
      send_prepare(lp, from, prepare_size, lp->default_value,
                   lp->default_value_len);
    } else {
      fill_holes(lp, from, prepare_size, lp->default_value,
                 lp->default_value_len);
    }
  }
}

static inline int handle_paxos_messages(struct paxos_hdr *paxos_hdr,
                                        struct app_lcore_params_worker *lp) {
  uint8_t msgtype = paxos_hdr->msgtype;
  switch (msgtype) {
  case PAXOS_RESET: {
    RTE_LOG(DEBUG, P4XOS, "Worker %u Reset instance %u\n", lp->worker_id,
            rte_be_to_cpu_32(paxos_hdr->inst));
    return TO_DROP;
  }
  case PAXOS_PREPARE: {
    prepare_handler(paxos_hdr, lp);
    break;
  }
  case PAXOS_ACCEPT: {
    accept_handler(paxos_hdr, lp);
    break;
  }
  case PAXOS_PROMISE: {
    promise_handler(paxos_hdr, lp);
    break;
  }
  case PAXOS_ACCEPTED: {
    accepted_handler(paxos_hdr, lp);
    break;
  }
  default: {
    RTE_LOG(DEBUG, P4XOS, "No handler for %u. Worker %u Tail pointer %u\n",
            msgtype, paxos_hdr->worker_id,
            rte_be_to_cpu_16(paxos_hdr->log_index));
    return NO_HANDLER;
  }
  }
  return SUCCESS;
}

int replica_handler(struct rte_mbuf *pkt_in, void *arg) {
  struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
  lp->total_pkts++;
  lp->total_bytes += pkt_in->pkt_len + PREAMBLE_CRC_IPG;
  int ret = filter_packets(pkt_in);
  if (ret < 0) {
    RTE_LOG(DEBUG, P4XOS, "Drop packets. Code %d\n", ret);
    return ret;
  }

  size_t paxos_offset = get_paxos_offset();
  struct paxos_hdr *paxos_hdr =
      rte_pktmbuf_mtod_offset(pkt_in, struct paxos_hdr *, paxos_offset);

  ret = handle_paxos_messages(paxos_hdr, lp);
  if (ret < 0) {
    return ret;
  }

  respond(pkt_in);

  return SUCCESS;
}
