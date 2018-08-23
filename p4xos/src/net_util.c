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

#include "dpp_paxos.h"
#include "learner.h"
#include "main.h"
#include "paxos.h"
#include "net_util.h"


void set_ether_hdr(struct ether_hdr *eth, uint16_t ethtype,
                          const struct ether_addr *src,
                          const struct ether_addr *dst) {
    eth->ether_type = rte_cpu_to_be_16(ethtype);
    ether_addr_copy(src, &eth->s_addr);
    ether_addr_copy(dst, &eth->d_addr);
}

void
set_ipv4_hdr(struct ipv4_hdr *ip, uint8_t proto, uint32_t src, uint32_t dst,
                uint16_t total_length)
{
    ip->version_ihl = 0x45;
    ip->total_length = rte_cpu_to_be_16(total_length);
    ip->packet_id = rte_cpu_to_be_16(0);
    ip->fragment_offset = rte_cpu_to_be_16(IPV4_HDR_DF_FLAG);
    ip->time_to_live = 64;
    ip->next_proto_id = proto;
    ip->hdr_checksum = 0;
    ip->src_addr = src;
    ip->dst_addr = dst;
}

void set_udp_hdr(struct udp_hdr *udp, uint16_t src_port,
                        uint16_t dst_port, uint16_t dgram_len) {
    udp->src_port = rte_cpu_to_be_16(src_port);
    udp->dst_port = rte_cpu_to_be_16(dst_port);
    udp->dgram_len = rte_cpu_to_be_16(dgram_len);
    udp->dgram_cksum = 0;
}

void set_udp_hdr_sockaddr_in(struct udp_hdr *udp, struct sockaddr_in *src,
                        struct sockaddr_in *dst, uint16_t dgram_len) {
    udp->src_port = src->sin_port;
    udp->dst_port = dst->sin_port;
    udp->dgram_len = rte_cpu_to_be_16(dgram_len);
    udp->dgram_cksum = 0;
}

void set_paxos_hdr(struct paxos_hdr *px, uint8_t msgtype, uint32_t inst,
                          uint16_t rnd, uint8_t worker_id, uint16_t acptid,
                          uint32_t request_id, char *value, int size) {
    uint64_t igress_ts = 0;
    px->msgtype = msgtype;
    px->inst = rte_cpu_to_be_32(inst);
    px->rnd = rte_cpu_to_be_16(rnd);
    px->vrnd = rte_cpu_to_be_16(0);
    px->acptid = rte_cpu_to_be_16(acptid);
    px->worker_id = worker_id;
    px->request_id = rte_cpu_to_be_32(request_id);
    char *pval = (char *) &px->value;
    if (size > 0 && value != NULL) {
        rte_memcpy(pval, value, size);
    }
    px->reserved = 0;
    px->igress_ts = rte_cpu_to_be_64(igress_ts);
}


int filter_packets(struct rte_mbuf *pkt_in) {
  struct ether_hdr *eth = rte_pktmbuf_mtod_offset(pkt_in, struct ether_hdr *, 0);
  size_t ip_offset = sizeof(struct ether_hdr);

  if (rte_be_to_cpu_16(eth->ether_type) != ETHER_TYPE_IPv4) {
      return NON_ETHERNET_PACKET;
  }

  struct ipv4_hdr *ip = rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);

  if (ip->next_proto_id != IPPROTO_UDP)
    return NON_UDP_PACKET;

  size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
  struct udp_hdr *udp = rte_pktmbuf_mtod_offset(pkt_in, struct udp_hdr *, udp_offset);

  return udp->dst_port;
}


/* Convert bytes to Gbit */
double bytes_to_gbits(uint64_t bytes) {
    double t = bytes;
    t *= (double)8;
    t /= 1000 * 1000 * 1000;
    return t;
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


void prepare_paxos_message(struct rte_mbuf *created_pkt, uint16_t port,
                     struct sockaddr_in* src, struct sockaddr_in* dst, uint8_t msgtype,
                     uint32_t inst, uint16_t rnd, uint8_t worker_id,
                     uint16_t node_id, uint32_t request_id, char *value, int size) {

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

    set_paxos_hdr(px, msgtype, inst, rnd, worker_id, node_id, request_id, value, size);

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


void set_ip_addr(struct ipv4_hdr *ip, uint32_t src, uint32_t dst) {
  ip->dst_addr = dst;
  ip->src_addr = src;
}
