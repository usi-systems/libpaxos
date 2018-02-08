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

#include "main.h"
#include "dpdk_paxos.h"
#include "app_hdr.h"
#include "paxos.h"
#include "learner.h"

static uint8_t DEFAULT_KEY[] = "ABCDEFGH1234567";
static uint8_t DEFAULT_VALUE[] = "ABCDEFGH1234567";
static const char *dest_ips[4] = { 	"192.168.4.95", "192.168.4.96",
									"192.168.4.97", "192.168.4.98" };

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
	ip->next_proto_id = proto;
	ip->src_addr = rte_cpu_to_be_32(src);
	ip->dst_addr = rte_cpu_to_be_32(dst);
}

static void
set_udp_hdr(struct udp_hdr *udp, uint16_t src_port, uint16_t dst_port, uint16_t dgram_len) {
	udp->src_port = rte_cpu_to_be_16(src_port);
	udp->dst_port = rte_cpu_to_be_16(dst_port);
	udp->dgram_len = rte_cpu_to_be_16(dgram_len);
	udp->dgram_cksum = 0;
}

static void
set_app_hdr(struct app_hdr *ap, uint32_t inst) {
	ap->msg_type = inst % 2;
	ap->key_len = rte_cpu_to_be_32(sizeof(DEFAULT_KEY));
	ap->value_len = rte_cpu_to_be_32(sizeof(DEFAULT_VALUE));
	rte_memcpy(ap->key, DEFAULT_KEY, sizeof(DEFAULT_KEY));
	if (ap->msg_type == WRITE_OP) {
		rte_memcpy(ap->value, DEFAULT_VALUE, sizeof(DEFAULT_VALUE));
	}
}

static void
set_paxos_hdr(struct paxos_hdr *px, uint32_t inst) {
	px->msgtype = rte_cpu_to_be_16(PAXOS_ACCEPTED);
	px->inst = rte_cpu_to_be_32(inst);
	px->rnd = rte_cpu_to_be_16(0);
	px->vrnd = rte_cpu_to_be_16(0);
	px->acptid = rte_cpu_to_be_16(0);
	px->value_len = rte_cpu_to_be_32(sizeof(struct app_hdr));
	px->igress_ts = rte_cpu_to_be_64(0);
	px->egress_ts = rte_cpu_to_be_64(0);
	struct app_hdr *ap = &px->value;
	set_app_hdr(ap, inst);
}

static void
prepare_message(struct rte_mbuf *created_pkt, uint16_t port, uint32_t inst) {
	struct ether_hdr *eth = rte_pktmbuf_mtod_offset(created_pkt, struct ether_hdr *, 0);
	set_ether_hdr(eth, ETHER_TYPE_IPv4, &mac1_addr, &mac2_addr);
	size_t ip_offset = sizeof(struct ether_hdr);
	struct ipv4_hdr *ip = rte_pktmbuf_mtod_offset(created_pkt, struct ipv4_hdr *, ip_offset);

	struct sockaddr_in sa;
	struct sockaddr_in da;
	// store this IP address in sa:
	inet_pton(AF_INET, "192.168.4.4", &(sa.sin_addr));
	// store this IP address in da:
	inet_pton(AF_INET, dest_ips[port % sizeof(dest_ips)], &(da.sin_addr));
	set_ipv4_hdr(ip, IPPROTO_UDP, sa.sin_addr.s_addr, da.sin_addr.s_addr);

	size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);

	struct udp_hdr *udp = rte_pktmbuf_mtod_offset(created_pkt, struct udp_hdr *, udp_offset);
	size_t dgram_len = sizeof(struct udp_hdr) + sizeof(struct paxos_hdr);
	set_udp_hdr(udp, 12345, 54321, dgram_len);

	size_t paxos_offset = udp_offset + sizeof(struct udp_hdr);
	struct paxos_hdr *px = rte_pktmbuf_mtod_offset(created_pkt, struct paxos_hdr *, paxos_offset);
	set_paxos_hdr(px, inst);
	size_t pkt_size = paxos_offset + sizeof(struct paxos_hdr);

	created_pkt->data_len = pkt_size;
	created_pkt->pkt_len = pkt_size;
}

void
send_initial_requests(struct app_lcore_params_io *lp)
{
	uint32_t lcore = rte_lcore_id();
	int ret;
	uint16_t p;
	for (p = 0; p < lp->tx.n_nic_ports; p++) {
		uint16_t port = lp->tx.nic_ports[p];
		ret = rte_pktmbuf_alloc_bulk(app.lcore_params[lcore].pool,
				lp->tx.mbuf_out[port].array,
				APP_DEFAULT_BURST_SIZE_IO_TX_WRITE);
		if (ret)
			rte_panic("Cannot allocate bulk of mbufs\n");
		int i;
		for (i = 0; i < APP_DEFAULT_BURST_SIZE_IO_TX_WRITE; i++) {
			struct rte_mbuf* pkt = lp->tx.mbuf_out[port].array[i];
			if (pkt != NULL) {
				prepare_message(pkt, port, i);
			}
		}
		lp->tx.mbuf_out_flush[port] = 1;
		lp->tx.mbuf_out[port].n_mbufs = i;
	}
}

void
handle_paxos_message(struct app_lcore_params_worker *lp, struct rte_mbuf *pkt_in)
{
	// struct ether_hdr *eth = rte_pktmbuf_mtod_offset(pkt_in, struct ether_hdr *, 0);
	size_t ip_offset = sizeof(struct ether_hdr);
	// struct ipv4_hdr *ip = rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);
	size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
	// struct udp_hdr *udp = rte_pktmbuf_mtod_offset(pkt_in, struct udp_hdr *, udp_offset);
	size_t paxos_offset = udp_offset + sizeof(struct udp_hdr);
	struct paxos_hdr *paxos_hdr = rte_pktmbuf_mtod_offset(pkt_in, struct paxos_hdr *, paxos_offset);
	uint16_t msgtype = rte_be_to_cpu_16(paxos_hdr->msgtype);

	int ret;
	switch(msgtype)
	{
		case PAXOS_PROMISE: {
			int vsize = rte_be_to_cpu_32(paxos_hdr->value_len);
			struct paxos_promise promise = {
				.iid = rte_be_to_cpu_32(paxos_hdr->inst),
				.ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
				.value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
				.aid = rte_be_to_cpu_16(paxos_hdr->acptid),
				.value = {vsize, (char*)&paxos_hdr->value},
			};
			paxos_message pa;
			ret = learner_receive_promise(lp->learner, &promise, &pa.u.accept);
			if (ret) {
                // TODO: Send Accept messages to acceptors
			}
			break;
		}
		case PAXOS_ACCEPTED: {
			int vsize = rte_be_to_cpu_32(paxos_hdr->value_len);
			struct paxos_accepted ack = {
				.iid = rte_be_to_cpu_32(paxos_hdr->inst),
				.ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
				.value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
				.aid = rte_be_to_cpu_16(paxos_hdr->acptid),
				.value = {vsize, (char*)&paxos_hdr->value}
			};
			// printf("value len %u, inst %u, ballot %u, vballot %u aid %u ret %d\n",
			// vsize,
			// rte_be_to_cpu_32(paxos_hdr->inst),
			// rte_be_to_cpu_16(paxos_hdr->rnd),
			// rte_be_to_cpu_16(paxos_hdr->vrnd),
			// rte_be_to_cpu_16(paxos_hdr->acptid),
			// ret);

			learner_receive_accepted(lp->learner, &ack);
			paxos_accepted out;
			if (learner_deliver_next(lp->learner, &out)) {
				lp->deliver(out.iid, out.value.paxos_value_val,
						out.value.paxos_value_len, lp);
			}
			break;
		}
		default:
			printf("No handler for %u\n", msgtype);
	}

}
