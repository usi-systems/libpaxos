#ifndef _NET_UTIL_H_
#define _NET_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>


static const struct ether_addr mac2_addr = {
    .addr_bytes = {0x08, 0x22, 0x22, 0x22, 0x22, 0x08}};

void set_ether_hdr(struct ether_hdr *eth, uint16_t ethtype,
                    const struct ether_addr *src,
                    const struct ether_addr *dst);

void set_ipv4_hdr(struct ipv4_hdr *ip, uint8_t proto, uint32_t src,
                    uint32_t dst, uint16_t total_length);

void set_udp_hdr(struct udp_hdr *udp, uint16_t src_port,
                    uint16_t dst_port, uint16_t dgram_len);

void set_udp_hdr_sockaddr_in(struct udp_hdr *udp, struct sockaddr_in *src,
                        struct sockaddr_in *dst, uint16_t dgram_len);

void set_ip_addr(struct ipv4_hdr *ip, uint32_t src, uint32_t dst);

void print_ips(uint32_t *src_ip, uint32_t *dst_ip);

int handle_arp_packet(struct rte_mbuf *pkt, struct ether_hdr *eth_hdr,
                        size_t offset);

int handle_icmp_packet(struct rte_mbuf *pkt, struct ether_hdr *eth_hdr,
        struct ipv4_hdr *ipv4_hdr, size_t offset);

int handle_ip_packet(struct rte_mbuf *pkt, struct ether_hdr *eth_hdr, size_t offset);

int pre_process(struct rte_mbuf *pkt);

#ifdef __cplusplus
}
#endif

#endif
