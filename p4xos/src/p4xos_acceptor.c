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

#include "paxos.h"
#include "learner.h"
#include "acceptor.h"
#include "main.h"
#include "dpp_paxos.h"
#include "net_util.h"


int prepare_handler(struct paxos_hdr *paxos_hdr, void *arg)
{
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    struct paxos_prepare prepare = {
        .iid = rte_be_to_cpu_32(paxos_hdr->inst),
        .ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
    };
    paxos_message out;
    if (acceptor_receive_prepare(lp->acceptor, &prepare, &out) != 0) {
        paxos_hdr->msgtype = out.type;
        paxos_hdr->vrnd = rte_cpu_to_be_16(out.u.promise.value_ballot);
        paxos_hdr->acptid = rte_cpu_to_be_16(app.p4xos_conf.node_id);
        if (out.u.promise.value.paxos_value_len) {
            rte_memcpy(&paxos_hdr->value, out.u.promise.value.paxos_value_val,
                out.u.promise.value.paxos_value_len);
        }
    } else {
        RTE_LOG(INFO, P4XOS, "Leader Prepare Failed\n");
        paxos_hdr->msgtype = LEARNER_PREPARE;
        iid_t highest_inst = learner_get_highest_instance(lp->learner);
        paxos_hdr->inst = rte_cpu_to_be_32(highest_inst);
    }
    return SUCCESS;
}

int accept_handler(struct paxos_hdr *paxos_hdr, void *arg)
{
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    int vsize = PAXOS_VALUE_SIZE;
    struct paxos_accept accept = {
        .iid = rte_be_to_cpu_32(paxos_hdr->inst),
        .ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
        .value = {vsize, (char *)&paxos_hdr->value}
    };
    paxos_message out;
    if (acceptor_receive_accept(lp->acceptor, &accept, &out) != 0) {
        paxos_hdr->msgtype = out.type;
        paxos_hdr->acptid = rte_cpu_to_be_16(app.p4xos_conf.node_id);
        lp->accepted_count++;
        RTE_LOG(DEBUG, P4XOS, "Worker %u Accepted instance %u balot %u\n",
            lp->worker_id, out.u.accepted.iid, out.u.accepted.ballot);
    } else {
        return DROP_ORIGINAL_PACKET;
    }
    return SUCCESS;
}
