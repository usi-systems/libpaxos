#ifndef _NET_UTIL_H_
#define _NET_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>


static const struct ether_addr mac1_addr = {
    .addr_bytes = {0x08, 0x11, 0x11, 0x11, 0x11, 0x08}};

static const struct ether_addr mac2_addr = {
    .addr_bytes = {0x08, 0x22, 0x22, 0x22, 0x22, 0x08}};

void set_ether_hdr(struct ether_hdr *eth, uint16_t ethtype,
                    const struct ether_addr *src,
                    const struct ether_addr *dst);
void set_ipv4_hdr(struct ipv4_hdr *ip, uint8_t proto, uint32_t src, uint32_t dst);
void set_udp_hdr(struct udp_hdr *udp, uint16_t src_port,
                    uint16_t dst_port, uint16_t dgram_len);

#ifdef __cplusplus
}
#endif

#endif
