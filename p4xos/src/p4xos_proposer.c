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

#include "acceptor.h"
#include "dpp_paxos.h"
#include "learner.h"
#include "proposer.h"
#include "main.h"
#include "paxos.h"
#include "net_util.h"

#define PREAMBLE_CRC_IPG 24


static inline int proposer_chosen_handler(struct paxos_hdr *paxos_hdr,
                                 struct app_lcore_params_worker *lp) {
    uint32_t inst = rte_be_to_cpu_32(paxos_hdr->inst);
    uint64_t now = 0;
    RTE_LOG(DEBUG, P4XOS, "Received Chosen instance %u\n", inst);

    if (app.p4xos_conf.measure_latency) {
        uint64_t previous = rte_be_to_cpu_64(paxos_hdr->igress_ts);
        if (previous > lp->start_ts) {
            now = rte_get_timer_cycles();
            uint64_t diff = now - previous;
            lp->latency += diff;
            lp->nb_latency++;
            paxos_hdr->igress_ts = rte_cpu_to_be_64(now);
            double latency = cycles_to_ns(diff, app.hz);
            lp->buffer_count += sprintf(&lp->file_buffer[lp->buffer_count], "%u %.0f\n",
                                            app.p4xos_conf.osd, latency);
            if (lp->buffer_count >= CHUNK_SIZE) {
                fwrite(lp->file_buffer, lp->buffer_count, 1, lp->latency_fp);
                lp->buffer_count = 0;
            }
        }
    }
    size_t vsize = PAXOS_VALUE_SIZE;
    lp->deliver(lp->worker_id, inst, (char *)&paxos_hdr->value, vsize, lp->deliver_arg);
    lp->nb_delivery++;
    paxos_hdr->msgtype = app.p4xos_conf.msgtype;

    if (app.p4xos_conf.measure_latency) {
        if (unlikely(inst % app.p4xos_conf.ts_interval == 0))
        {
            if(likely(now == 0)) {
                now = rte_get_timer_cycles();
            }
            paxos_hdr->igress_ts = rte_cpu_to_be_64(now);
        } else {
            paxos_hdr->igress_ts = 0;
        }
    }

    return SUCCESS;
}


int proposer_handler(struct rte_mbuf *pkt_in, void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    paxos_stats(pkt_in, lp);
    size_t ip_offset = sizeof(struct ether_hdr);
    struct ipv4_hdr *ip =
    rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);
    size_t paxos_offset = get_paxos_offset();
    struct paxos_hdr *paxos_hdr =
            rte_pktmbuf_mtod_offset(pkt_in, struct paxos_hdr *, paxos_offset);
    uint8_t msgtype = paxos_hdr->msgtype;

    switch (msgtype) {
        case PAXOS_CHOSEN: {
        proposer_chosen_handler(paxos_hdr, lp);
        set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr,
            app.p4xos_conf.paxos_leader.sin_addr.s_addr);
        break;
    }
    default:
        RTE_LOG(DEBUG, P4XOS, "No handler for %u\n", msgtype);
        return NO_HANDLER;
    }

    size_t data_size = sizeof(struct paxos_hdr);
    prepare_hw_checksum(pkt_in, data_size);
#ifdef RESUBMIT
    rte_timer_reset(&lp->recv_timer, app.hz/RESUBMIT_TIMEOUT,
            SINGLE, lp->lcore_id, proposer_resubmit, lp);
#endif
    return SUCCESS;
}


void proposer_resubmit(struct rte_timer *timer, void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    uint16_t port = app.p4xos_conf.tx_port;
    uint32_t n_mbufs = app.p4xos_conf.osd - lp->mbuf_out[port].n_mbufs;
    submit_bulk(lp->worker_id, n_mbufs, lp, NULL, 0);
    RTE_LOG(INFO, P4XOS, "Worker %u Timeout. Resumit %u packets\n",
        lp->worker_id, app.p4xos_conf.osd);
    rte_timer_reset(timer, app.hz/RESUBMIT_TIMEOUT, SINGLE,
        lp->lcore_id, proposer_resubmit, lp);
}
