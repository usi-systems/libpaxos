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
#include <rte_hexdump.h>
#include <rte_interrupts.h>
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

#include <rte_ether.h>
#include <rte_arp.h>
#include <rte_ip.h>
#include <rte_icmp.h>
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
                          uint32_t request_id, uint64_t igress_ts, char *value, int size) {
    px->msgtype = msgtype;
    px->worker_id = worker_id;
    px->inst = rte_cpu_to_be_32(inst);
    px->rnd = rte_cpu_to_be_16(rnd);
    px->vrnd = rte_cpu_to_be_16(0);
    px->acptid = rte_cpu_to_be_16(acptid);
    uint16_t log_index = (uint16_t)(inst & 0xff);
    px->log_index = rte_cpu_to_be_16(log_index);
    px->reserved = 0;
    char *pval = (char *) &px->value;
    if (size > 0 && value != NULL) {
        rte_memcpy(pval, value, size);
    }
    px->request_id = rte_cpu_to_be_32(request_id);
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
                     uint16_t node_id, uint32_t request_id, uint64_t igress_ts, char *value, int size) {

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

    set_paxos_hdr(px, msgtype, inst, rnd, worker_id, node_id, request_id, igress_ts, value, size);

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

void print_ips(uint32_t *src_ip, uint32_t *dst_ip)
{
    char src[INET_ADDRSTRLEN];
    char dst[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, src_ip, src, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, dst_ip, dst, INET_ADDRSTRLEN);
    RTE_LOG(DEBUG, P4XOS, "%s -> %s\n", src, dst);
}

int handle_arp_packet(struct rte_mbuf *pkt, struct ether_hdr *eth_hdr, size_t offset)
{
    struct arp_hdr *arp_hdr;
    arp_hdr = rte_pktmbuf_mtod_offset(pkt, struct arp_hdr *, offset);
    print_ips(&arp_hdr->arp_data.arp_sip, &arp_hdr->arp_data.arp_tip);
    if (arp_hdr->arp_data.arp_tip == app.p4xos_conf.mine.sin_addr.s_addr) {
        if (arp_hdr->arp_op == rte_cpu_to_be_16(ARP_OP_REQUEST)) {
            RTE_LOG(DEBUG, P4XOS, "ARP Request\n");
            arp_hdr->arp_op = rte_cpu_to_be_16(ARP_OP_REPLY);
            /* Switch src and dst data and set bonding MAC */
            ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
            rte_eth_macaddr_get(pkt->port, &eth_hdr->s_addr);
            ether_addr_copy(&arp_hdr->arp_data.arp_sha, &arp_hdr->arp_data.arp_tha);
            arp_hdr->arp_data.arp_tip = arp_hdr->arp_data.arp_sip;
            ether_addr_copy(&eth_hdr->s_addr, &arp_hdr->arp_data.arp_sha);
            arp_hdr->arp_data.arp_sip = app.p4xos_conf.mine.sin_addr.s_addr;
            return SUCCESS;
        }
    }

    return TO_DROP;
}

int
handle_icmp_packet(struct rte_mbuf *pkt, struct ether_hdr *eth_hdr,
        struct ipv4_hdr *ipv4_hdr, size_t offset)
{
    struct icmp_hdr *icmp_hdr = rte_pktmbuf_mtod_offset(pkt, struct icmp_hdr *, offset);
    RTE_LOG(DEBUG, P4XOS, "ICMP Type: %02x\n", icmp_hdr->icmp_type);
    if (icmp_hdr->icmp_type == IP_ICMP_ECHO_REQUEST) {
        if (ipv4_hdr->dst_addr == app.p4xos_conf.mine.sin_addr.s_addr) {
            icmp_hdr->icmp_type = IP_ICMP_ECHO_REPLY;
            ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
            rte_eth_macaddr_get(pkt->port, &eth_hdr->s_addr);
            ipv4_hdr->dst_addr = ipv4_hdr->src_addr;
            ipv4_hdr->src_addr = app.p4xos_conf.mine.sin_addr.s_addr;
            return SUCCESS;
        }
    }
    return TO_DROP;
}

int handle_udp_packet(struct rte_mbuf *pkt, __rte_unused struct ether_hdr *ether_hdr,
        __rte_unused struct ipv4_hdr *ipv4_hdr, size_t offset)
{
    struct udp_hdr *udp_hdr = rte_pktmbuf_mtod_offset(pkt, struct udp_hdr *, offset);
    if (rte_be_to_cpu_16(udp_hdr->dst_port) == P4XOS_PORT)
        return PAXOS_PACKET;
    else
        return NON_PAXOS_PACKET;
}

int
handle_ip_packet(struct rte_mbuf *pkt, struct ether_hdr *eth_hdr, size_t offset)
{
    struct ipv4_hdr *ipv4_hdr;
    ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *, offset);
    size_t l4_offset = offset + sizeof(struct ipv4_hdr);
    print_ips(&ipv4_hdr->src_addr, &ipv4_hdr->dst_addr);

    switch (ipv4_hdr->next_proto_id) {
        case IPPROTO_UDP:
            return handle_udp_packet(pkt, eth_hdr, ipv4_hdr, l4_offset);
        case IPPROTO_ICMP:
            return handle_icmp_packet(pkt, eth_hdr, ipv4_hdr, l4_offset);
        default:
            RTE_LOG(DEBUG, P4XOS, "IP Proto: %d\n", ipv4_hdr->next_proto_id);
            return NO_HANDLER;
    }
}


int
pre_process(struct rte_mbuf *pkt)
{
    struct ether_hdr *eth_hdr = rte_pktmbuf_mtod_offset(pkt, struct ether_hdr *, 0);
    size_t l3_offset = sizeof(struct ether_hdr);
    uint16_t ether_type = rte_be_to_cpu_16(eth_hdr->ether_type);
    switch (ether_type) {
        case ETHER_TYPE_ARP:
            return handle_arp_packet(pkt, eth_hdr, l3_offset);
        case ETHER_TYPE_IPv4:
            return handle_ip_packet(pkt, eth_hdr, l3_offset);
        default:
            RTE_LOG(DEBUG, P4XOS, "Ether Proto: 0x%04x\n", ether_type);
            return NO_HANDLER;
    }
    return TO_DROP;
}
