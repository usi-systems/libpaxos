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

#include "paxos.h"
#include "learner.h"
#include "main.h"
#include "dpdk_paxos.h"

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
