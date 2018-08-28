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
#include <rte_malloc.h>

#include "acceptor.h"
#include "dpp_paxos.h"
#include "learner.h"
#include "proposer.h"
#include "main.h"
#include "paxos.h"
#include "net_util.h"


void resubmit_request(struct resubmit_parm *parm);

static int get_timer_idex(uint32_t request_id)
{
    return request_id % app.p4xos_conf.osd;
}

static inline int proposer_chosen_handler(struct paxos_hdr *paxos_hdr,
                                 struct app_lcore_params_worker *lp) {
    uint32_t inst = rte_be_to_cpu_32(paxos_hdr->inst);
    uint64_t now = 0;
    uint32_t request_id = rte_be_to_cpu_32(paxos_hdr->request_id);
    if (request_id <= 0) {
        RTE_LOG(INFO, P4XOS, "Invalid request_id\n");
        return DROP_ORIGINAL_PACKET;
    }
    if (app.p4xos_conf.measure_latency) {
        uint64_t previous = rte_be_to_cpu_64(paxos_hdr->igress_ts);
        if (previous > lp->start_ts) {
            now = rte_get_timer_cycles();
            uint64_t diff = now - previous;
            lp->latency += diff;
            lp->nb_latency++;
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
    /* New Request */
    paxos_hdr->msgtype = app.p4xos_conf.msgtype;
    if (app.p4xos_conf.measure_latency) {
        if (unlikely(inst % app.p4xos_conf.ts_interval == 0))
        {
            if (likely(now == 0)) {
                now = rte_get_timer_cycles();
            }
            paxos_hdr->igress_ts = rte_cpu_to_be_64(now);
        } else {
            paxos_hdr->igress_ts = 0;
        }
    }

#ifdef RESUBMIT
    uint32_t idx = get_timer_idex(request_id);
    lp->resubmit_params[idx]->igress_ts = now;
    lp->resubmit_params[idx]->request_id += app.p4xos_conf.osd;
    paxos_hdr->request_id = rte_cpu_to_be_32(lp->resubmit_params[idx]->request_id);
    rte_timer_reset(&lp->request_timer[idx], app.hz/RESUBMIT_TIMEOUT,
        SINGLE, lp->lcore_id, proposer_resubmit, lp->resubmit_params[idx]);
#endif

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

    print_paxos_hdr(paxos_hdr);
    uint8_t msgtype = paxos_hdr->msgtype;
    int ret;
    switch (msgtype) {
        case PAXOS_CHOSEN: {
        ret = proposer_chosen_handler(paxos_hdr, lp);
        if (ret != SUCCESS) {
            return ret;
        }
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
    return SUCCESS;
}


void proposer_resubmit(struct rte_timer *timer, void *arg) {
    struct resubmit_parm *parm = (struct resubmit_parm *)arg;
    resubmit_request(parm);
    RTE_LOG(INFO, P4XOS, "Worker %u Resubmit Request %u\n",
            parm->lp->worker_id, parm->request_id);
    rte_timer_reset(timer, app.hz/RESUBMIT_TIMEOUT, SINGLE,
        parm->lp->lcore_id, proposer_resubmit, parm);
}

void resubmit_request(struct resubmit_parm *parm) {
    uint32_t mbuf_idx;
    uint16_t port = app.p4xos_conf.tx_port;
    int lcore = app_get_lcore_worker(parm->lp->worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }

    struct rte_mbuf *pkt = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);

    prepare_paxos_message(pkt, port, &app.p4xos_conf.mine,
                    &app.p4xos_conf.paxos_leader,
                    app.p4xos_conf.msgtype, 0, 0, parm->lp->worker_id,
                    app.p4xos_conf.node_id, parm->request_id, parm->igress_ts,
                    parm->value, parm->vsize);

    mbuf_idx = parm->lp->mbuf_out[port].n_mbufs;
    parm->lp->mbuf_out[port].array[mbuf_idx++] = pkt;
    parm->lp->mbuf_out[port].n_mbufs = mbuf_idx;


    uint32_t idx = get_timer_idex(parm->request_id);
    rte_timer_reset(&parm->lp->request_timer[idx], app.hz/RESUBMIT_TIMEOUT,
                    SINGLE, parm->lp->lcore_id, proposer_resubmit, parm);
}


void submit_bulk(uint8_t worker_id, uint32_t nb_pkts,
    struct app_lcore_params_worker *lp, char *value, int size) {
    int ret;
    uint32_t mbuf_idx;
    uint16_t port = app.p4xos_conf.tx_port;
    int lcore = app_get_lcore_worker(worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }

    struct rte_mbuf *pkts[nb_pkts];
    ret = rte_pktmbuf_alloc_bulk(app.lcore_params[lcore].pool, pkts, nb_pkts);

    if (ret < 0) {
        RTE_LOG(INFO, USER1, "Not enough entries in the mempools\n");
        return;
    }

    uint32_t i;
    for (i = 0; i < nb_pkts; i++) {
        uint32_t request_id = lp->request_id + i;
        uint64_t igress_ts = rte_get_timer_cycles();
        prepare_paxos_message(pkts[i], port, &app.p4xos_conf.mine,
                        &app.p4xos_conf.paxos_leader,
                        app.p4xos_conf.msgtype, 0, 0, worker_id,
                        app.p4xos_conf.node_id, request_id, igress_ts, value, size);

        mbuf_idx = lp->mbuf_out[port].n_mbufs;
        lp->mbuf_out[port].array[mbuf_idx++] = pkts[i];
        lp->mbuf_out[port].n_mbufs = mbuf_idx;

#ifdef RESUBMIT
        struct resubmit_parm *parm = rte_zmalloc("resubmit_parm", sizeof(struct resubmit_parm), 0);
        if (parm == NULL) {
            rte_panic("Cannot allocate memory for resubmit_parm\n");
        }
        parm->request_id = request_id;
        parm->lp = lp;
        parm->value = value;
        parm->vsize = size;
        parm->igress_ts = igress_ts;
        uint32_t idx = get_timer_idex(request_id);
        lp->resubmit_params[idx] = parm;
        rte_timer_reset(&lp->request_timer[idx], app.hz/RESUBMIT_TIMEOUT,
                        SINGLE, lp->lcore_id, proposer_resubmit, parm);
#endif
    }

    lp->mbuf_out_flush[port] = 1;
}

void free_resubmit_params(struct resubmit_parm *parm)
{
    if (!parm)
        return;
    rte_free(parm);
}
