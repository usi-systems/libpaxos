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
#include "proposer.h"
#include "main.h"
#include "paxos.h"
#include "net_util.h"

#define PREAMBLE_CRC_IPG 24

int proposer_prepare_allocated(struct app_lcore_params_worker *lp,
                                struct paxos_hdr *out);

static inline void respond(struct rte_mbuf *pkt_in);
double cycles_to_ns(uint64_t cycles, uint64_t hz);

/* Convert cycles to ns */
inline double cycles_to_ns(uint64_t cycles, uint64_t hz) {
  double t = cycles;
  t *= (double)NS_PER_S;
  t /= hz;
  return t;
}




static inline void respond(struct rte_mbuf *pkt_in) {
    size_t ip_offset = sizeof(struct ether_hdr);
    struct ipv4_hdr *ip = rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);
    char src[INET_ADDRSTRLEN];
    char dst[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ip->src_addr), src, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip->dst_addr), dst, INET_ADDRSTRLEN);
    RTE_LOG(DEBUG, P4XOS, "%s => %s\n", src, dst);
    set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr, ip->src_addr);
    size_t data_size = sizeof(struct paxos_hdr);
    prepare_hw_checksum(pkt_in, data_size);
}


size_t get_paxos_offset(void) {
  return sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) +
         sizeof(struct udp_hdr);
}



void print_paxos_hdr(struct paxos_hdr *paxos_hdr) {
    RTE_LOG(DEBUG, P4XOS, "msgtype %u worker_id %u round %u inst %u log_index %u vrnd %u"
            "acptid %u reserved %u value %s request_id %u igress_ts %"PRIu64"\n",
                paxos_hdr->msgtype,
                paxos_hdr->worker_id,
                rte_be_to_cpu_16(paxos_hdr->rnd),
                rte_be_to_cpu_32(paxos_hdr->inst),
                rte_be_to_cpu_16(paxos_hdr->log_index),
                rte_be_to_cpu_16(paxos_hdr->vrnd),
                rte_be_to_cpu_16(paxos_hdr->acptid),
                rte_be_to_cpu_16(paxos_hdr->reserved),
                (char*)&paxos_hdr->value,
                rte_be_to_cpu_32(paxos_hdr->request_id),
                rte_be_to_cpu_64(paxos_hdr->igress_ts));
}


void paxos_stats(struct rte_mbuf *pkt_in, struct app_lcore_params_worker *lp) {
  lp->total_pkts++;
  lp->total_bytes += pkt_in->pkt_len + PREAMBLE_CRC_IPG;
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
                        app.p4xos_conf.node_id, lp->request_id, 0, value, size);
    }
    lp->mbuf_out[port].n_mbufs++;
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
