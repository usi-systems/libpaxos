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
#include <math.h>

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
#include "net_util.h"

#define PREAMBLE_CRC_IPG 24

#define TIMESTAMP_FREQ 1023
/* Convert bytes to Gbit */
inline double bytes_to_gbits(uint64_t bytes) {
  double t = bytes;
  t *= (double)8;
  t /= 1000 * 1000 * 1000;
  return t;
}

/* Convert cycles to ns */
inline double cycles_to_ns(uint64_t cycles, uint64_t hz) {
  double t = cycles;

  t *= (double)NS_PER_S;
  t /= hz;
  return t;
}



static void set_ip_addr(struct ipv4_hdr *ip, uint32_t src, uint32_t dst) {
  ip->dst_addr = dst;
  ip->src_addr = src;
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

void print_ipv6(struct ipv6_hdr *ip_hdr) {
    int i = 0;
    RTE_LOG(DEBUG, P4XOS, "Proto %u. ", ip_hdr->proto);
    RTE_LOG(DEBUG, P4XOS, "\nSrc addr: ");
    for (i = 0; i < 16; i+=2) {
        RTE_LOG(DEBUG, P4XOS, "%02X%02X:", ip_hdr->src_addr[i], ip_hdr->src_addr[i+1]);
    }
    RTE_LOG(DEBUG, P4XOS, "\nDst addr: ");
    for (i = 0; i < 16; i+=2) {
        RTE_LOG(DEBUG, P4XOS, "%02X%02X:", ip_hdr->dst_addr[i], ip_hdr->dst_addr[i+1]);
    }
    RTE_LOG(DEBUG, P4XOS, "\n");
}

int filter_packets(struct rte_mbuf *pkt_in) {
  struct ether_hdr *eth =
      rte_pktmbuf_mtod_offset(pkt_in, struct ether_hdr *, 0);
  size_t ip_offset = sizeof(struct ether_hdr);
  if (rte_be_to_cpu_16(eth->ether_type) != ETHER_TYPE_IPv4) {
      // if (rte_be_to_cpu_16(eth->ether_type) == ETHER_TYPE_IPv6) {
      //     struct ipv6_hdr *ipv6 = rte_pktmbuf_mtod_offset(pkt_in, struct ipv6_hdr *, ip_offset);
      //     print_ipv6(ipv6);
      // }
      return NON_ETHERNET_PACKET;
  }
  struct ipv4_hdr *ip =
      rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);
  if (ip->next_proto_id != IPPROTO_UDP)
    return NON_UDP_PACKET;
  size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
  struct udp_hdr *udp =
      rte_pktmbuf_mtod_offset(pkt_in, struct udp_hdr *, udp_offset);
  // if (rte_be_to_cpu_16(udp->dst_port) != P4XOS_PORT)
  //   return NON_PAXOS_PACKET;
  return udp->dst_port;
}

static inline void respond(struct rte_mbuf *pkt_in) {
  size_t ip_offset = sizeof(struct ether_hdr);
  struct ipv4_hdr *ip =
      rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);
  char src[INET_ADDRSTRLEN];
  char dst[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(ip->src_addr), src, INET_ADDRSTRLEN);
  inet_ntop(AF_INET, &(ip->dst_addr), dst, INET_ADDRSTRLEN);
  RTE_LOG(DEBUG, P4XOS, "%s => %s\n", src, dst);
  set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr, ip->src_addr);
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
      RTE_LOG(DEBUG, P4XOS, "Worker %u Receive Prepare instance %u\n", lp->worker_id,
              rte_be_to_cpu_32(paxos_hdr->inst));
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
  RTE_LOG(DEBUG, P4XOS, "Worker %u Receive Accept ballot %u instance %u\n", lp->worker_id,
          accept.ballot, accept.iid);

  if (acceptor_receive_accept(lp->acceptor, &accept, &out) != 0) {
    paxos_hdr->msgtype = out.type;
    paxos_hdr->acptid = rte_cpu_to_be_16(app.p4xos_conf.node_id);
    lp->accepted_count++;
    RTE_LOG(DEBUG, P4XOS, "Worker %u Accepted instance %u balot %u\n", lp->worker_id,
            out.u.accepted.iid, out.u.accepted.ballot);
  } else {
    return NO_MAJORITY;
  }
  return SUCCESS;
}

static inline int promise_handler(struct paxos_hdr *paxos_hdr,
                                  struct app_lcore_params_worker *lp) {
    int ret = 0;
    int vsize = PAXOS_VALUE_SIZE; // rte_be_to_cpu_32(paxos_hdr->value_len);

    uint32_t inst = rte_be_to_cpu_32(paxos_hdr->inst);
    if (likely(inst) > lp->cur_inst) {
        lp->cur_inst = inst;
    }

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
        RTE_LOG(DEBUG, P4XOS, "Worker %u Send Accept inst %u ballot %u\n",
                lp->worker_id, pa.u.accept.iid, pa.u.accept.ballot);
        paxos_hdr->msgtype = PAXOS_ACCEPT;
        paxos_hdr->rnd = rte_cpu_to_be_16(pa.u.accept.ballot);
        if (pa.u.accept.value.paxos_value_val) {
            rte_memcpy((void*)&paxos_hdr->value,
                        (void*)&pa.u.accept.value.paxos_value_val,
                        pa.u.accept.value.paxos_value_len);
        }
            return SUCCESS;
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
    int vsize = PAXOS_VALUE_SIZE; // rte_be_to_cpu_32(paxos_hdr->value_len);
    uint32_t inst = rte_be_to_cpu_32(paxos_hdr->inst);
    if (inst > lp->cur_inst) {
        lp->cur_inst = inst;
    }
    struct paxos_accepted ack = {.iid = rte_be_to_cpu_32(paxos_hdr->inst),
                               .ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
                               .value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
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
        /* Leader does not deliver accepted messages */
        // lp->deliver(lp->worker_id, out.iid, out.value.paxos_value_val,
        //             out.value.paxos_value_len, lp->deliver_arg);
        paxos_hdr->msgtype = PAXOS_CHOSEN;
        paxos_accepted_destroy(&out);
    } else {
        return NO_MAJORITY;
    }
    return SUCCESS;
}


static inline int
learner_new_command_handler(struct paxos_hdr *paxos_hdr,
                            struct app_lcore_params_worker *lp) {
    lp->cur_inst++;
    RTE_LOG(DEBUG, P4XOS, "Worker %u: Received NEW_COMMAND\n", lp->worker_id);

    paxos_prepare out;
    learner_prepare(lp->learner, &out, lp->cur_inst);
    paxos_hdr->msgtype = PAXOS_PREPARE;
    paxos_hdr->inst = rte_cpu_to_be_32(out.iid);
    paxos_hdr->rnd = rte_cpu_to_be_16(out.ballot);
    return SUCCESS;
}

static inline int chosen_handler(struct paxos_hdr *paxos_hdr,
                                 struct app_lcore_params_worker *lp) {
    uint32_t inst = rte_be_to_cpu_32(paxos_hdr->inst);
    uint64_t now = 0;
    RTE_LOG(DEBUG, P4XOS, "Received Chosen instance %u\n", inst);

    if (app.p4xos_conf.measure_latency) {
        uint64_t previous = rte_be_to_cpu_64(paxos_hdr->igress_ts);
        if (previous > lp->start_ts) {
            now = rte_get_timer_cycles();
            uint64_t diff = now - previous;
            lp->latency += diff;
            lp->nb_latency++;
            paxos_hdr->igress_ts = rte_cpu_to_be_64(now);
            double latency = cycles_to_ns(diff, app.hz);
            lp->buffer_count += sprintf(&lp->file_buffer[lp->buffer_count], "%u %.0f\n",
                                            app.p4xos_conf.osd, latency);
            if (lp->buffer_count >= CHUNK_SIZE) {
                fwrite(lp->file_buffer, lp->buffer_count, 1, lp->latency_fp);
                lp->buffer_count = 0;
            }
        }
    }
    size_t vsize = PAXOS_VALUE_SIZE;
    lp->deliver(lp->worker_id, inst, (char *)&paxos_hdr->value, vsize, lp->deliver_arg);
    lp->nb_delivery++;
    paxos_hdr->msgtype = app.p4xos_conf.msgtype;

    if (app.p4xos_conf.measure_latency) {
        if (unlikely(inst % TIMESTAMP_FREQ == 0))
        {
            if(likely(now == 0)) {
                now = rte_get_timer_cycles();
            }
            paxos_hdr->igress_ts = rte_cpu_to_be_64(now);
        } else {
            paxos_hdr->igress_ts = 0;
        }
    }

    return SUCCESS;
}

void print_paxos_hdr(struct paxos_hdr *paxos_hdr) {
    printf("msgtype %u worker_id %u round %u inst %u log_index %u vrnd %u \
            acptid %u reserved %u value %s reserved2 %u igress_ts %"PRIu64"\n",
                paxos_hdr->msgtype,
                paxos_hdr->worker_id,
                rte_be_to_cpu_16(paxos_hdr->rnd),
                rte_be_to_cpu_32(paxos_hdr->inst),
                rte_be_to_cpu_16(paxos_hdr->log_index),
                rte_be_to_cpu_16(paxos_hdr->vrnd),
                rte_be_to_cpu_16(paxos_hdr->acptid),
                rte_be_to_cpu_16(paxos_hdr->reserved),
                (char*)&paxos_hdr->value,
                rte_be_to_cpu_32(paxos_hdr->reserved2),
                rte_be_to_cpu_64(paxos_hdr->igress_ts));
}

static inline int learner_chosen_handler(struct paxos_hdr *paxos_hdr,
                                         struct app_lcore_params_worker *lp) {
  int vsize = PAXOS_VALUE_SIZE; // rte_be_to_cpu_32(paxos_hdr->value_len);
  uint32_t inst = rte_be_to_cpu_32(paxos_hdr->inst);
  if (inst > lp->highest_chosen_inst) {
    lp->highest_chosen_inst = inst;
  }

  RTE_LOG(DEBUG, P4XOS, "Worker %u, log_index %u, Chosen instance %u\n",
          lp->worker_id, rte_be_to_cpu_16(paxos_hdr->log_index), inst);
  lp->deliver(lp->worker_id, inst, (char *)&paxos_hdr->value,
              vsize, lp->deliver_arg);
  return SUCCESS;
}

static inline int new_command_handler(struct paxos_hdr *paxos_hdr,
                                      struct app_lcore_params_worker *lp) {
  paxos_hdr->inst = rte_cpu_to_be_32(lp->cur_inst++);
  paxos_hdr->msgtype = PAXOS_ACCEPT;
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
#ifdef RESUBMIT
    rte_timer_reset(&lp->recv_timer[lp->lcore_id], app.hz/RESUBMIT_TIMEOUT,
            SINGLE, lp->lcore_id, proposer_resubmit, lp);
#endif
    return SUCCESS;
}


void proposer_resubmit(struct rte_timer *timer, void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    uint16_t port = app.p4xos_conf.tx_port;
    uint32_t n_mbufs = app.p4xos_conf.osd - lp->mbuf_out[port].n_mbufs;
    submit_bulk(lp->worker_id, n_mbufs, lp, NULL, 0);
    RTE_LOG(INFO, P4XOS, "Worker %u Timeout. Resumit %u packets\n",
        lp->worker_id, app.p4xos_conf.osd);
    rte_timer_reset(&lp->recv_timer[lp->lcore_id], app.hz/RESUBMIT_TIMEOUT, SINGLE,
        lp->lcore_id, proposer_resubmit, lp);
}

void timer_send_checkpoint(struct rte_timer *timer, void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    RTE_LOG(DEBUG, P4XOS, "Worker %u timeout. Sent checkpoint instance %u\n",
        lp->worker_id, lp->highest_chosen_inst);
    send_checkpoint_message(lp->worker_id, lp->highest_chosen_inst);
    rte_timer_reset(&lp->recv_timer[lp->lcore_id], app.hz, SINGLE, lp->lcore_id,
       timer_send_checkpoint, lp);
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
            send_prepare(lp, from, prepare_size, lp->default_value, lp->default_value_len);
        } else {
            fill_holes(lp, from, prepare_size, lp->default_value,
                        lp->default_value_len);
        }
    }
}

static inline int handle_paxos_messages(struct rte_mbuf *pkt_in,
                                        struct app_lcore_params_worker *lp) {
    size_t ip_offset = sizeof(struct ether_hdr);
    struct ipv4_hdr *ip =
    rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);

    char src[INET_ADDRSTRLEN];
    char dst[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ip->src_addr), src, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip->dst_addr), dst, INET_ADDRSTRLEN);
    RTE_LOG(DEBUG, P4XOS, "%s => %s\n", src, dst);

    size_t paxos_offset = get_paxos_offset();
    struct paxos_hdr *paxos_hdr =
      rte_pktmbuf_mtod_offset(pkt_in, struct paxos_hdr *, paxos_offset);
    int ret;

    uint8_t msgtype = paxos_hdr->msgtype;
    switch (msgtype) {
    case PAXOS_RESET: {
        RTE_LOG(DEBUG, P4XOS, "Worker %u Reset instance %u\n", lp->worker_id,
                                            rte_be_to_cpu_32(paxos_hdr->inst));
        return TO_DROP;
        }
        case NEW_COMMAND: {
            app.p4xos_conf.client.sin_addr.s_addr = ip->src_addr;
            ret = learner_new_command_handler(paxos_hdr, lp);
            set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr,
                app.p4xos_conf.acceptor_addr.sin_addr.s_addr);
        break;
        }
        case PAXOS_PREPARE: {
            prepare_handler(paxos_hdr, lp);
            set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr, ip->src_addr);
        break;
        }
        case PAXOS_ACCEPT: {
            accept_handler(paxos_hdr, lp);
            set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr, ip->src_addr);
        break;
        }
        case PAXOS_PROMISE: {
            ret = promise_handler(paxos_hdr, lp);
            set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr,
                app.p4xos_conf.acceptor_addr.sin_addr.s_addr);
        break;
        }
        case PAXOS_ACCEPTED: {
            ret = accepted_handler(paxos_hdr, lp);
            set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr,
                app.p4xos_conf.learner_addr.sin_addr.s_addr);
        break;
        }
        case PAXOS_CHOSEN: {
            ret = learner_chosen_handler(paxos_hdr, lp);
            if (!app.p4xos_conf.respond_to_client) {
                rte_pktmbuf_free(pkt_in);
                return DROP_ORIGINAL_PACKET;
            }
            set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr,
                app.p4xos_conf.client.sin_addr.s_addr);
        break;
        }
        default: {
            RTE_LOG(DEBUG, P4XOS, "No handler for %u. Worker %u Tail pointer %u\n",
            msgtype, paxos_hdr->worker_id,
            rte_be_to_cpu_16(paxos_hdr->log_index));
            return NO_HANDLER;
        }
    }
    size_t data_size = sizeof(struct paxos_hdr);
    prepare_hw_checksum(pkt_in, data_size);
    rte_timer_reset(&lp->recv_timer[lp->lcore_id], app.hz, SINGLE, lp->lcore_id,
                    timer_send_checkpoint, lp);
    return ret;
}

int replica_handler(struct rte_mbuf *pkt_in, void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    lp->total_pkts++;
    lp->total_bytes += pkt_in->pkt_len + PREAMBLE_CRC_IPG;
    int ret = SUCCESS;
    ret = handle_paxos_messages(pkt_in, lp);
    return ret;
}


void prepare_paxos_message(struct rte_mbuf *created_pkt, uint16_t port,
                     struct sockaddr_in* src, struct sockaddr_in* dst, uint8_t msgtype,
                     uint32_t inst, uint16_t rnd, uint8_t worker_id,
                     uint16_t node_id, char *value, int size) {

    struct ether_hdr *eth =
                    rte_pktmbuf_mtod_offset(created_pkt, struct ether_hdr *, 0);
    struct ether_addr addr;
    rte_eth_macaddr_get(port, &addr);
    set_ether_hdr(eth, ETHER_TYPE_IPv4, &addr, &mac2_addr);
    size_t ip_offset = sizeof(struct ether_hdr);
    struct ipv4_hdr *ip =
            rte_pktmbuf_mtod_offset(created_pkt, struct ipv4_hdr *, ip_offset);

    size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
    struct udp_hdr *udp =
            rte_pktmbuf_mtod_offset(created_pkt, struct udp_hdr *, udp_offset);

    size_t dgram_len = sizeof(struct udp_hdr) + sizeof(struct paxos_hdr);
    size_t paxos_offset = udp_offset + sizeof(struct udp_hdr);
    struct paxos_hdr *px =
        rte_pktmbuf_mtod_offset(created_pkt, struct paxos_hdr *, paxos_offset);

    set_paxos_hdr(px, msgtype, inst, rnd, worker_id, node_id, value, size);

    size_t data_size = sizeof(struct paxos_hdr);
    size_t l4_len = sizeof(struct udp_hdr) + data_size;
    size_t pkt_size = paxos_offset + sizeof(struct paxos_hdr);

    set_udp_hdr_sockaddr_in(udp, src, dst, dgram_len);
    udp->dgram_len = rte_cpu_to_be_16(l4_len);
    set_ipv4_hdr(ip, IPPROTO_UDP, src->sin_addr.s_addr, dst->sin_addr.s_addr, pkt_size);
    created_pkt->data_len = pkt_size;
    created_pkt->pkt_len = pkt_size;
    created_pkt->l2_len = sizeof(struct ether_hdr);
    created_pkt->l3_len = sizeof(struct ipv4_hdr);
    created_pkt->l4_len = l4_len;
    created_pkt->ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
    udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, created_pkt->ol_flags);
}

void reset_leader_instance(uint32_t worker_id) {
    uint16_t port = app.p4xos_conf.tx_port;
    struct app_lcore_params_worker *lp = app_get_worker(worker_id);
    int lcore = app_get_lcore_worker(worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }
    uint32_t mbuf_idx = lp->mbuf_out[port].n_mbufs;
    struct rte_mbuf *pkt = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);
    if (pkt != NULL) {
        prepare_paxos_message(pkt, port, &app.p4xos_conf.mine,
                        &app.p4xos_conf.paxos_leader, PAXOS_RESET, 0, 0, worker_id,
                        app.p4xos_conf.node_id, NULL, 0);
    }
    lp->mbuf_out[port].array[mbuf_idx] = pkt;
    lp->mbuf_out[port].n_mbufs++;
}

void send_prepare(struct app_lcore_params_worker *lp, uint32_t inst,
                  uint32_t prepare_size, char *value, int size) {
    int ret;
    uint32_t i;
    uint32_t mbuf_idx;
    uint16_t port = app.p4xos_conf.tx_port;
    int lcore = app_get_lcore_worker(lp->worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }
    struct rte_mbuf *pkts[prepare_size];
    ret = rte_pktmbuf_alloc_bulk(app.lcore_params[lcore].pool, pkts, prepare_size);

    if (ret < 0) {
        RTE_LOG(INFO, USER1, "Not enough entries in the mempools for ACCEPT\n");
        return;
    }

    for (i = 0; i < prepare_size; i++) {
        paxos_prepare out;
        learner_prepare(lp->learner, &out, inst + i);
        RTE_LOG(DEBUG, P4XOS, "Worker %u Send Prepare instance %u ballot %u\n",
                lp->worker_id, out.iid, out.ballot);
        prepare_paxos_message(pkts[i], port, &app.p4xos_conf.mine,
            &app.p4xos_conf.acceptor_addr, PAXOS_PREPARE, out.iid,
            out.ballot, lp->worker_id, app.p4xos_conf.node_id, value, size);

        mbuf_idx = lp->mbuf_out[port].n_mbufs;
        lp->mbuf_out[port].array[mbuf_idx++] = pkts[i];
        lp->mbuf_out[port].n_mbufs = mbuf_idx;
    }
}

void fill_holes(struct app_lcore_params_worker *lp, uint32_t inst,
                uint32_t prepare_size, char *value, int size) {
    int ret;
    uint32_t i;
    uint32_t mbuf_idx;
    uint16_t port = app.p4xos_conf.tx_port;
    int lcore = app_get_lcore_worker(lp->worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }
    struct rte_mbuf *pkts[prepare_size];
    ret = rte_pktmbuf_alloc_bulk(app.lcore_params[lcore].pool, pkts, prepare_size);

    if (ret < 0) {
        RTE_LOG(INFO, USER1, "Not enough entries in the mempools for ACCEPT\n");
        return;
    }

    for (i = 0; i < prepare_size; i++) {
        prepare_paxos_message(pkts[i], port, &app.p4xos_conf.mine,
                        &app.p4xos_conf.paxos_leader, PAXOS_ACCEPT, inst + i, 0,
                        lp->worker_id, app.p4xos_conf.node_id, value, size);

        mbuf_idx = lp->mbuf_out[port].n_mbufs;
        lp->mbuf_out[port].array[mbuf_idx++] = pkts[i];
        lp->mbuf_out[port].n_mbufs = mbuf_idx;
    }
}

void send_accept(struct app_lcore_params_worker *lp, paxos_accept *accept) {
    uint16_t port = app.p4xos_conf.tx_port;
    int lcore = app_get_lcore_worker(lp->worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }
    struct rte_mbuf *pkt = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);
    if (pkt == NULL) {
        RTE_LOG(INFO, USER1, "Not enough entries in the mempools for ACCEPT\n");
        return;
    }

    char *value = accept->value.paxos_value_val;
    int size = accept->value.paxos_value_len;
    if (value == NULL) {
        value = lp->default_value;
        size = lp->default_value_len;
    }
    RTE_LOG(DEBUG, P4XOS, "Worker %u Send Accept inst %u ballot %u\n",
        lp->worker_id, accept->iid, accept->ballot);
    prepare_paxos_message(pkt, port, &app.p4xos_conf.mine,
                    &app.p4xos_conf.acceptor_addr, PAXOS_ACCEPT, accept->iid,
                    accept->ballot, lp->worker_id, app.p4xos_conf.node_id, value,
                    size);

    uint32_t mbuf_idx = lp->mbuf_out[port].n_mbufs;
    lp->mbuf_out[port].array[mbuf_idx++] = pkt;
    lp->mbuf_out[port].n_mbufs = mbuf_idx;
}

void submit(uint8_t worker_id, char *value, int size) {
    uint16_t port = app.p4xos_conf.tx_port;
    struct app_lcore_params_worker *lp = app_get_worker(worker_id);
    int lcore = app_get_lcore_worker(worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }

    uint32_t mbuf_idx = lp->mbuf_out[port].n_mbufs;
    lp->mbuf_out[port].array[mbuf_idx] = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);
    struct rte_mbuf *pkt = lp->mbuf_out[port].array[mbuf_idx];
    if (pkt != NULL) {
        prepare_paxos_message(pkt, port, &app.p4xos_conf.mine,
                        &app.p4xos_conf.paxos_leader,
                        app.p4xos_conf.msgtype, lp->cur_inst++, 0, worker_id,
                        app.p4xos_conf.node_id, value, size);
    }
    lp->mbuf_out[port].n_mbufs++;
}

void submit_bulk(uint8_t worker_id, uint32_t nb_pkts,
    struct app_lcore_params_worker *lp, char *value, int size) {
    int ret;
    uint32_t mbuf_idx;
    uint16_t port = app.p4xos_conf.tx_port;
    int lcore = app_get_lcore_worker(worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }

    struct rte_mbuf *pkts[nb_pkts];
    ret = rte_pktmbuf_alloc_bulk(app.lcore_params[lcore].pool, pkts, nb_pkts);

    if (ret < 0) {
        RTE_LOG(INFO, USER1, "Not enough entries in the mempools\n");
        return;
    }

    uint32_t i;
    for (i = 0; i < nb_pkts; i++) {
        prepare_paxos_message(pkts[i], port, &app.p4xos_conf.mine,
                        &app.p4xos_conf.paxos_leader,
                        app.p4xos_conf.msgtype, 0, 0, worker_id,
                        app.p4xos_conf.node_id, value, size);
        value += size;
        mbuf_idx = lp->mbuf_out[port].n_mbufs;
        lp->mbuf_out[port].array[mbuf_idx++] = pkts[i];
        lp->mbuf_out[port].n_mbufs = mbuf_idx;
    }
    lp->mbuf_out_flush[port] = 1;
}

void submit_bulk_priority(uint8_t worker_id, uint32_t nb_pkts, char *value, int size)
{
    int ret;
    uint32_t n_mbufs;
    uint16_t port = app.p4xos_conf.tx_port;
    uint32_t lcore;
    if (app_get_lcore_for_nic_tx(port, &lcore) < 0) {
        rte_panic("Error: get lcore tx\n");
        return;
    }
    struct app_lcore_params_io *lp_io = &app.lcore_params[lcore].io;

    struct rte_mbuf *pkts[nb_pkts];
    ret = rte_pktmbuf_alloc_bulk(app.lcore_params[lcore].pool, pkts, nb_pkts);

    if (ret < 0) {
        RTE_LOG(INFO, USER1, "Not enough entries in the mempools\n");
        return;
    }

    uint32_t i;
    for (i = 0; i < nb_pkts; i++) {
        prepare_paxos_message(pkts[i], port, &app.p4xos_conf.mine,
                        &app.p4xos_conf.paxos_leader,
                        app.p4xos_conf.msgtype, 0, 0, worker_id,
                        app.p4xos_conf.node_id, value, size);
        value += size;
        n_mbufs = lp_io->tx.mbuf_out[port].n_mbufs;
        lp_io->tx.mbuf_out[port].array[n_mbufs++] = pkts[i];
        lp_io->tx.mbuf_out[port].n_mbufs = n_mbufs;
    }
}


void send_checkpoint_message(uint8_t worker_id, uint32_t inst) {
    uint16_t port = app.p4xos_conf.tx_port;
    uint32_t lcore;
    if (app_get_lcore_for_nic_tx(port, &lcore) < 0) {
        rte_panic("Error: get lcore tx\n");
        return;
    }
    struct app_lcore_params_io *lp = &app.lcore_params[lcore].io;

    uint32_t n_mbufs = lp->tx.mbuf_out[port].n_mbufs;
    struct rte_mbuf *pkt = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);
    if (pkt != NULL) {
        prepare_paxos_message(pkt, port, &app.p4xos_conf.mine,
                        &app.p4xos_conf.acceptor_addr,
                        LEARNER_CHECKPOINT, inst, 0, worker_id,
                        app.p4xos_conf.node_id, NULL, 0);
    }
    lp->tx.mbuf_out[port].array[n_mbufs] = pkt;
    lp->tx.mbuf_out[port].n_mbufs++;
    // app_send_burst(port, lp->tx.mbuf_out[port].array, n_mbufs);
}



static void
copy_buffer_to_pkt(struct rte_mbuf *pkt, uint16_t port, uint8_t pid, char* buffer,
    uint32_t buffer_size, struct sockaddr_in *from, struct sockaddr_in *to)
{
    struct ether_hdr *eth = rte_pktmbuf_mtod_offset(pkt, struct ether_hdr *, 0);
    eth->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
    rte_eth_macaddr_get(port, &eth->s_addr);
    ether_addr_copy(&mac2_addr, &eth->d_addr);

    size_t ip_offset = sizeof(struct ether_hdr);
    struct ipv4_hdr *ip = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *, ip_offset);

    ip->version_ihl = 0x45;
    ip->packet_id = rte_cpu_to_be_16(0);
    ip->fragment_offset = rte_cpu_to_be_16(IPV4_HDR_DF_FLAG);
    ip->time_to_live = 64;
    ip->next_proto_id = IPPROTO_UDP;
    ip->hdr_checksum = 0;
    ip->src_addr = from->sin_addr.s_addr;
    ip->dst_addr = to->sin_addr.s_addr;

    size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
    struct udp_hdr *udp = rte_pktmbuf_mtod_offset(pkt, struct udp_hdr *, udp_offset);

    udp->src_port = from->sin_port;
    udp->dst_port = to->sin_port;

    size_t payload_offset = udp_offset + sizeof(struct udp_hdr);
    char* req = rte_pktmbuf_mtod_offset(pkt, char*, payload_offset);
    // HARDCODE MSGTYPE
    *req = 5;
    // HARDCODE Partition
    *(req + 1) = pid;
    *(req + 2) = rte_cpu_to_be_32(buffer_size);
    rte_memcpy(req+6, buffer, buffer_size);
    size_t dgram_len =  sizeof(struct udp_hdr) + 6 + buffer_size;
    printf("Buffer size %u Dgram len %zu\n", buffer_size, dgram_len);
    set_udp_hdr(udp, 12345, 39012, dgram_len);
    size_t pkt_size = udp_offset + dgram_len;
    ip->total_length = rte_cpu_to_be_16(sizeof(struct ipv4_hdr) + dgram_len);
    udp->dgram_len = rte_cpu_to_be_16(dgram_len);
    udp->dgram_cksum = 0;
    pkt->data_len = pkt_size;
    pkt->pkt_len = pkt_size;
    pkt->l2_len = sizeof(struct ether_hdr);
    pkt->l3_len = sizeof(struct ipv4_hdr);
    pkt->l4_len = dgram_len;
    pkt->ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
    udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, pkt->ol_flags);
}


int net_sendto(uint8_t worker_id, char* buf, size_t len, struct sockaddr_in *to)
{
    uint16_t port = app.p4xos_conf.tx_port;
    uint32_t mbuf_idx;
    int lcore = app_get_lcore_worker(worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }

    struct app_lcore_params_worker *lp = app_get_worker(worker_id);
    struct rte_mbuf *pkt = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);

    if (pkt == NULL)
    {
        RTE_LOG(WARNING, P4XOS, "Not enough entries in the mempools\n");
        return -1;
    }

    copy_buffer_to_pkt(pkt, port, worker_id, buf, len, &app.p4xos_conf.mine, to);
    mbuf_idx = lp->mbuf_out[port].n_mbufs;
    lp->mbuf_out[port].array[mbuf_idx++] = pkt;
    lp->mbuf_out[port].n_mbufs = mbuf_idx;
    lp->mbuf_out_flush[port] = 1;

    return 0;
}
