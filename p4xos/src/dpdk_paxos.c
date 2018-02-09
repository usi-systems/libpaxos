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
#include "learner.h"
#include "main.h"
#include "dpp_paxos.h"

static void
swap_ips(struct ipv4_hdr *ip) {
	uint32_t tmp = ip->dst_addr;
	ip->dst_addr = ip->src_addr;
	ip->src_addr = tmp;
}

static void
swap_udp_ports(struct udp_hdr *udp) {
	uint16_t tmp = udp->dst_port;
	udp->dst_port = udp->src_port;
	udp->src_port = tmp;
	udp->dgram_cksum = 0;
}


static size_t get_paxos_offset(void) {
	return sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr);
}

static void prepare_hw_checksum(struct rte_mbuf *pkt_in, size_t data_size) {
	// struct ether_hdr *eth = rte_pktmbuf_mtod_offset(pkt_in, struct ether_hdr *, 0);
	size_t ip_offset = sizeof(struct ether_hdr);
	struct ipv4_hdr *ip = rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);
	swap_ips(ip);
	size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
	struct udp_hdr *udp = rte_pktmbuf_mtod_offset(pkt_in, struct udp_hdr *, udp_offset);
	swap_udp_ports(udp);
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

void
proposer_handler(struct rte_mbuf *pkt_in, void *arg)
{
	struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
	size_t paxos_offset = get_paxos_offset();
	struct paxos_hdr *paxos_hdr = rte_pktmbuf_mtod_offset(pkt_in, struct paxos_hdr *, paxos_offset);
	size_t data_size = sizeof(struct paxos_hdr);
	prepare_hw_checksum(pkt_in, data_size);
	uint16_t msgtype = rte_be_to_cpu_16(paxos_hdr->msgtype);
	int ret;
	switch(msgtype)
	{
		case PAXOS_ACCEPTED: {
			uint32_t cur_inst = rte_be_to_cpu_32(paxos_hdr->inst);
			paxos_hdr->inst = rte_cpu_to_be_32(cur_inst + APP_DEFAULT_BURST_SIZE_WORKER_WRITE);
			uint64_t now = rte_get_timer_cycles();
			uint64_t latency = now - rte_be_to_cpu_64(paxos_hdr->igress_ts);
			lp->latency += latency;
			lp->nb_delivery ++;
			if (lp->nb_delivery >= 1000000) {
				double avg_cycle_latency = (double) lp->latency / (double) lp->nb_delivery;
				double avg_ns_latency = avg_cycle_latency * NS_PER_S / rte_get_timer_hz();
				printf("Avg latency = %.2f cycles ~ %.1f ns\n", avg_cycle_latency, avg_ns_latency);
				lp->latency = 0;
				lp->nb_delivery = 0;
			}
			paxos_hdr->igress_ts = rte_cpu_to_be_64(now);
			break;
		}
		default:
			printf("No handler for %u\n", msgtype);
	}
}

void
learner_handler(struct rte_mbuf *pkt_in, void *arg)
{
	struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
	size_t paxos_offset = get_paxos_offset();
	struct paxos_hdr *paxos_hdr = rte_pktmbuf_mtod_offset(pkt_in, struct paxos_hdr *, paxos_offset);
	size_t data_size = sizeof(struct paxos_hdr);
	prepare_hw_checksum(pkt_in, data_size);
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

			learner_receive_accepted(lp->learner, &ack);
			paxos_accepted out;
			if (learner_deliver_next(lp->learner, &out)) {
				lp->deliver(out.iid, out.value.paxos_value_val,
						out.value.paxos_value_len, lp->deliver_arg);
			}
			break;
		}
		default:
			printf("No handler for %u\n", msgtype);
	}

}
