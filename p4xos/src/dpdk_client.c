#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <arpa/inet.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_lpm.h>
#include <rte_hexdump.h>

#include "paxos.h"
#include "main.h"
#include "dpp_paxos.h"

static const struct ether_addr mac1_addr = {
	.addr_bytes= { 0x08, 0x11, 0x11, 0x11, 0x11, 0x08 }
};

static const struct ether_addr mac2_addr = {
	.addr_bytes= { 0x08, 0x22, 0x22, 0x22, 0x22, 0x08 }
};

static void
set_ether_hdr(struct ether_hdr *eth, uint16_t ethtype,
	const struct ether_addr *src, const struct ether_addr *dst) {

	eth->ether_type = rte_cpu_to_be_16(ethtype);
	ether_addr_copy(src, &eth->s_addr);
	ether_addr_copy(dst, &eth->d_addr);
}

static void
set_ipv4_hdr(struct ipv4_hdr *ip, uint8_t proto, uint32_t src, uint32_t dst) {
	ip->version_ihl = 0x45;
	ip->total_length = rte_cpu_to_be_16 (sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr) + sizeof(struct paxos_hdr));
	ip->packet_id = rte_cpu_to_be_16(rte_rdtsc());
	ip->fragment_offset = rte_cpu_to_be_16(IPV4_HDR_DF_FLAG);
	ip->time_to_live = 64;
	ip->next_proto_id = proto;
	ip->hdr_checksum = 0;
	ip->src_addr = src;
	ip->dst_addr = dst;
	// rte_hexdump(stdout, "IP", ip, sizeof(struct ipv4_hdr));

}

static void
set_udp_hdr(struct udp_hdr *udp, uint16_t src_port, uint16_t dst_port, uint16_t dgram_len) {
	udp->src_port = rte_cpu_to_be_16(src_port);
	udp->dst_port = rte_cpu_to_be_16(dst_port);
	udp->dgram_len = rte_cpu_to_be_16(dgram_len);
	udp->dgram_cksum = 0;
	// rte_hexdump(stdout, "UDP", udp, sizeof(struct udp_hdr));

}

static void
set_paxos_hdr(struct paxos_hdr *px, uint16_t msgtype, uint32_t inst, char* value, int size) {
	uint64_t igress_ts = 0;
	px->msgtype = rte_cpu_to_be_16(msgtype);
	px->inst = rte_cpu_to_be_32(inst);
	px->rnd = rte_cpu_to_be_16(0);
	px->vrnd = rte_cpu_to_be_16(0);
	px->acptid = rte_cpu_to_be_16(0);
	px->value_len = rte_cpu_to_be_32(size);
	if (size > 0 && value != NULL) {
		rte_memcpy(&px->value, value, size);
	}
	if (app.p4xos_conf.osd > 1) {
		igress_ts = (inst % (app.p4xos_conf.osd-1) == 0) ? rte_get_timer_cycles() : 0;
	} else {
		igress_ts = rte_get_timer_cycles();
	}
	px->igress_ts = rte_cpu_to_be_64(igress_ts);
	px->egress_ts = rte_cpu_to_be_64(0);
}

static void
prepare_message(struct rte_mbuf *created_pkt, uint16_t port, uint32_t src_addr,
		uint32_t dst_addr, uint16_t msgtype, uint32_t inst, char* value, int size) {

	struct ether_hdr *eth = rte_pktmbuf_mtod_offset(created_pkt, struct ether_hdr *, 0);
	set_ether_hdr(eth, ETHER_TYPE_IPv4, &mac1_addr, &mac2_addr);
	size_t ip_offset = sizeof(struct ether_hdr);
	struct ipv4_hdr *ip = rte_pktmbuf_mtod_offset(created_pkt, struct ipv4_hdr *, ip_offset);

	set_ipv4_hdr(ip, IPPROTO_UDP, src_addr, dst_addr);

	size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);

	struct udp_hdr *udp = rte_pktmbuf_mtod_offset(created_pkt, struct udp_hdr *, udp_offset);
	size_t dgram_len = sizeof(struct udp_hdr) + sizeof(struct paxos_hdr);
	set_udp_hdr(udp, 12345, P4XOS_PORT, dgram_len);

	size_t paxos_offset = udp_offset + sizeof(struct udp_hdr);
	struct paxos_hdr *px = rte_pktmbuf_mtod_offset(created_pkt, struct paxos_hdr *, paxos_offset);
	set_paxos_hdr(px, msgtype, inst, value, size);
	size_t data_size = sizeof(struct paxos_hdr);
	size_t l4_len = sizeof(struct udp_hdr) + data_size;
	size_t pkt_size = paxos_offset + sizeof(struct paxos_hdr);
	udp->dgram_len = rte_cpu_to_be_16(l4_len);
	created_pkt->data_len = pkt_size;
	created_pkt->pkt_len = pkt_size;
	created_pkt->l2_len = sizeof(struct ether_hdr);
	created_pkt->l3_len = sizeof(struct ipv4_hdr);
	created_pkt->l4_len = l4_len;
	created_pkt->ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
	udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, created_pkt->ol_flags);
}

void reset_leader_instance(void)
{
    uint32_t lcore;
    uint16_t port = app.p4xos_conf.tx_port;
    app_get_lcore_for_nic_tx(port, &lcore);
    struct app_lcore_params_io *lp = &app.lcore_params[lcore].io;
	uint32_t mbuf_idx = lp->tx.mbuf_out[port].n_mbufs;
	if (mbuf_idx >= APP_MBUF_ARRAY_SIZE) {
		return;
	}
	lp->tx.mbuf_out[port].array[mbuf_idx] = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);
	struct rte_mbuf* pkt = lp->tx.mbuf_out[port].array[mbuf_idx];
	if (pkt != NULL) {
		prepare_message(pkt, port, app.p4xos_conf.src_addr, app.p4xos_conf.dst_addr, PAXOS_RESET, mbuf_idx, NULL, 0);
	}
	lp->tx.mbuf_out_flush[port] = 1;
	lp->tx.mbuf_out[port].n_mbufs++;
}


void
send_accept(struct app_lcore_params_worker *lp, uint32_t inst, uint32_t prepare_size, char* value, int size)
{
	uint16_t port;
	uint32_t lcore_io;
	int ret;
	uint32_t pos;
	for (port = 1; port < APP_MAX_NIC_PORTS; port ++) {

		if (app.nic_tx_port_mask[port] == 0) {
			continue;
		}

		if (app_get_lcore_for_nic_tx(port, &lcore_io) < 0) {
			rte_panic("Algorithmic error (no I/O core to handle TX of port %u)\n",
				port);
		}

		struct rte_mbuf* prepare_pkts[prepare_size];
		ret = rte_pktmbuf_alloc_bulk(app.lcore_params[lcore_io].pool, prepare_pkts, prepare_size);

		if (ret < 0) {
			RTE_LOG(INFO, USER1, "Not enough entries in the mempools for PREPARE\n");
			return;
		}

		uint32_t i;
		for (i = 0; i < prepare_size; i++) {
			prepare_message(prepare_pkts[i], port, app.p4xos_conf.src_addr, app.p4xos_conf.dst_addr, PAXOS_ACCEPT, inst+i, value, size);

			pos = lp->mbuf_out[port].n_mbufs;

			lp->mbuf_out[port].array[pos ++] = prepare_pkts[i];
			lp->mbuf_out[port].n_mbufs = pos;
		}
		ret = rte_ring_sp_enqueue_bulk(
			lp->rings_out[port],
			(void **) lp->mbuf_out[port].array,
			prepare_size,
			NULL);

		if (unlikely(ret == 0)) {
			uint32_t k;
			for (k = 0; k < prepare_size; k ++) {
				struct rte_mbuf *pkt_to_free = lp->mbuf_out[port].array[k];
				rte_pktmbuf_free(pkt_to_free);
			}
		}
		lp->mbuf_out[port].n_mbufs = 0;
		lp->mbuf_out_flush[port] = 0;
	}
}


void submit(char* value, int size)
{
    uint32_t lcore;
    uint16_t port = app.p4xos_conf.tx_port;
    app_get_lcore_for_nic_tx(port, &lcore);
    struct app_lcore_params_io *lp = &app.lcore_params[lcore].io;
	uint32_t mbuf_idx = lp->tx.mbuf_out[port].n_mbufs;
	lp->tx.mbuf_out[port].array[mbuf_idx] = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);
	struct rte_mbuf* pkt = lp->tx.mbuf_out[port].array[mbuf_idx];
	if (pkt != NULL) {
		prepare_message(pkt, port, app.p4xos_conf.src_addr, app.p4xos_conf.dst_addr, app.p4xos_conf.msgtype, mbuf_idx, value, size);
	}
	lp->tx.mbuf_out_flush[port] = 1;
	lp->tx.mbuf_out[port].n_mbufs++;
}

void submit_all_ports(char* value, int size)
{
    uint32_t lcore;
    uint16_t port;
		struct app_lcore_params_io *lp;
		for (port = 0; port < APP_MAX_NIC_PORTS; port ++) {

			if (app.nic_tx_port_mask[port] == 0) {
				continue;
			}
			app_get_lcore_for_nic_tx(port, &lcore);
		    lp = &app.lcore_params[lcore].io;
			uint32_t mbuf_idx = lp->tx.mbuf_out[port].n_mbufs;
			lp->tx.mbuf_out[port].array[mbuf_idx] = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);
			struct rte_mbuf* pkt = lp->tx.mbuf_out[port].array[mbuf_idx];
			if (pkt != NULL) {
				if (port % 2)
					prepare_message(pkt, port, app.p4xos_conf.src_addr, app.p4xos_conf.dst_addr, app.p4xos_conf.msgtype, mbuf_idx, value, size);
				else
					prepare_message(pkt, port, app.p4xos_conf.dst_addr, app.p4xos_conf.src_addr, app.p4xos_conf.msgtype, mbuf_idx, value, size);
			}
			lp->tx.mbuf_out_flush[port] = 1;
			lp->tx.mbuf_out[port].n_mbufs++;
		}
}
