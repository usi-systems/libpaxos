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

#ifdef __cplusplus
}
#endif

#endif
