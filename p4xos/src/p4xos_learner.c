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

#define MAX_PREPARE_SIZE 8


int learner_checkpoint_handler(struct paxos_hdr *paxos_hdr, void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    uint32_t inst = rte_be_to_cpu_32(paxos_hdr->inst);
    RTE_LOG(DEBUG, P4XOS, "Worker %u, Checkpoint instance %u\n",
            lp->worker_id, inst);

    if (lp->proposer) {
        struct paxos_acceptor_state acc_state = { .trim_iid = inst };
        proposer_receive_acceptor_state(lp->proposer, &acc_state);
    }

    if (lp->acceptor) {
        paxos_trim trim = { .iid = inst };
        acceptor_receive_trim(lp->acceptor, &trim);
    }

    return DROP_ORIGINAL_PACKET;
}


void timer_send_checkpoint(struct rte_timer *timer, void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    uint32_t highest_delivered = learner_get_instance_id(lp->learner) - 1;
    RTE_LOG(DEBUG, P4XOS, "Worker %u checkpoint timer timeout.\n", lp->worker_id);
    send_checkpoint_message(lp->worker_id, highest_delivered);
    if (app.p4xos_conf.leader) {
        proposer_preexecute(lp);
    }
}


void learner_call_deliver(__rte_unused struct rte_timer *timer,
                          __rte_unused void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    paxos_accepted out;
    while (learner_deliver_next(lp->learner, &out)) {
        lp->deliver(lp->worker_id, out.iid, out.value.paxos_value_val,
        out.value.paxos_value_len, lp->deliver_arg);
        RTE_LOG(DEBUG, P4XOS, "Finished instance %u\n", out.iid);
        submit(lp->worker_id, out.value.paxos_value_val, out.value.paxos_value_len);
        paxos_accepted_destroy(&out);
    }
}


void learner_check_holes(struct app_lcore_params_worker *lp) {
    uint32_t from, to;
    if (learner_has_holes(lp->learner, &from, &to)) {
        lp->has_holes = 1;
        RTE_LOG(WARNING, P4XOS, "Learner %u Holes from %u to %u\n", lp->worker_id,
        from, to);
        uint32_t prepare_size = to - from;
        if (prepare_size > MAX_PREPARE_SIZE)
            prepare_size = MAX_PREPARE_SIZE;
        if (app.p4xos_conf.run_prepare) {
            send_prepare(lp, from, prepare_size, lp->default_value, lp->default_value_len);
        } else {
            fill_holes(lp, from, prepare_size, lp->default_value,
                        lp->default_value_len);
        }
    } else {
        lp->has_holes = 0;
    }
}


void learner_check_holes_cb(__rte_unused struct rte_timer *timer,
                         __rte_unused void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    learner_check_holes(lp);
}


void reset_leader_instance(uint32_t worker_id) {
    uint16_t port = app.p4xos_conf.tx_port;
    struct app_lcore_params_worker *lp = app_get_worker(worker_id);
    int lcore = app_get_lcore_worker(worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }
    uint32_t mbuf_idx = lp->mbuf_out[port].n_mbufs;
    struct rte_mbuf *pkt = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);
    if (pkt != NULL) {
        prepare_paxos_message(pkt, port, &app.p4xos_conf.mine,
                        &app.p4xos_conf.paxos_leader, PAXOS_RESET, 0, 0, worker_id,
                        app.p4xos_conf.node_id, 0, 0, NULL, 0);
    }
    lp->mbuf_out[port].array[mbuf_idx] = pkt;
    lp->mbuf_out[port].n_mbufs++;
}

void send_prepare(struct app_lcore_params_worker *lp, uint32_t inst,
                  uint32_t prepare_size, char *value, int size) {
    int ret;
    uint32_t i;
    uint32_t mbuf_idx;
    uint16_t port = app.p4xos_conf.tx_port;
    int lcore = app_get_lcore_worker(lp->worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }
    struct rte_mbuf *pkts[prepare_size];
    ret = rte_pktmbuf_alloc_bulk(app.lcore_params[lcore].pool, pkts, prepare_size);

    if (ret < 0) {
        RTE_LOG(INFO, P4XOS, "Not enough entries in the mempools for ACCEPT\n");
        return;
    }

    for (i = 0; i < prepare_size; i++) {
        paxos_prepare out;
        proposer_prepare_instance(lp->proposer, inst + i, &out);
        RTE_LOG(INFO, P4XOS, "Worker %u Send Prepare instance %u ballot %u\n",
                lp->worker_id, out.iid, out.ballot);
        prepare_paxos_message(pkts[i], port, &app.p4xos_conf.mine,
            &app.p4xos_conf.primary_replica, PAXOS_PREPARE, out.iid,
            out.ballot, lp->worker_id, app.p4xos_conf.node_id, 0, out.iid, value, size);

        mbuf_idx = lp->mbuf_out[port].n_mbufs;
        lp->mbuf_out[port].array[mbuf_idx++] = pkts[i];
        lp->mbuf_out[port].n_mbufs = mbuf_idx;
    }
}

void fill_holes(struct app_lcore_params_worker *lp, uint32_t inst,
                uint32_t prepare_size, char *value, int size) {
    int ret;
    uint32_t i;
    uint32_t mbuf_idx;
    uint16_t port = app.p4xos_conf.tx_port;
    int lcore = app_get_lcore_worker(lp->worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }
    struct rte_mbuf *pkts[prepare_size];
    ret = rte_pktmbuf_alloc_bulk(app.lcore_params[lcore].pool, pkts, prepare_size);

    if (ret < 0) {
        rte_panic("Not enough entries in the mempools for PAXOS_ACCEPT\n");
    }

    for (i = 0; i < prepare_size; i++) {
        prepare_paxos_message(pkts[i], port, &app.p4xos_conf.mine,
                        &app.p4xos_conf.acceptor_addr, PAXOS_ACCEPT, inst + i, 0,
                        lp->worker_id, app.p4xos_conf.node_id, 0, inst + i, value, size);

        mbuf_idx = lp->mbuf_out[port].n_mbufs;
        lp->mbuf_out[port].array[mbuf_idx++] = pkts[i];
        lp->mbuf_out[port].n_mbufs = mbuf_idx;
    }
}

void send_accept(struct app_lcore_params_worker *lp, paxos_accept *accept) {
    uint16_t port = app.p4xos_conf.tx_port;
    int lcore = app_get_lcore_worker(lp->worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }
    struct rte_mbuf *pkt = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);
    if (pkt == NULL) {
        rte_panic("Not enough entries in the mempools for PAXOS_ACCEPT\n");
    }

    char *value = accept->value.paxos_value_val;
    int size = accept->value.paxos_value_len;
    if (value == NULL) {
        value = lp->default_value;
        size = lp->default_value_len;
    }
    RTE_LOG(DEBUG, P4XOS, "Worker %u Send Accept inst %u ballot %u\n",
        lp->worker_id, accept->iid, accept->ballot);
    prepare_paxos_message(pkt, port, &app.p4xos_conf.mine,
                    &app.p4xos_conf.acceptor_addr, PAXOS_ACCEPT, accept->iid,
                    accept->ballot, lp->worker_id, app.p4xos_conf.node_id, 0, 0,
                    value, size);

    uint32_t mbuf_idx = lp->mbuf_out[port].n_mbufs;
    lp->mbuf_out[port].array[mbuf_idx++] = pkt;
    lp->mbuf_out[port].n_mbufs = mbuf_idx;
}


void send_checkpoint_message(uint8_t worker_id, uint32_t highest_delivered) {
    RTE_LOG(DEBUG, P4XOS, "Worker %u sent checkpoint instance %u\n",
        worker_id, highest_delivered);
    uint16_t port = app.p4xos_conf.tx_port;
    uint32_t lcore;
    if (app_get_lcore_for_nic_tx(port, &lcore) < 0) {
        rte_panic("Error: get lcore tx\n");
        return;
    }
    struct app_lcore_params_io *lp_io = &app.lcore_params[lcore].io;

    uint32_t n_mbufs = lp_io->tx.mbuf_out[port].n_mbufs;
    struct rte_mbuf *pkt = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);
    if (pkt != NULL) {
        prepare_paxos_message(pkt, port, &app.p4xos_conf.mine,
                        // &app.p4xos_conf.acceptor_addr,
                        &app.p4xos_conf.primary_replica,
                        LEARNER_CHECKPOINT, highest_delivered, 0, worker_id,
                        app.p4xos_conf.node_id, 0, highest_delivered, NULL, 0);
    }
    lp_io->tx.mbuf_out[port].array[n_mbufs] = pkt;
    lp_io->tx.mbuf_out[port].n_mbufs++;

    struct app_lcore_params_worker *lp = app_get_worker(worker_id);
    rte_timer_reset(&lp->checkpoint_timer, app.hz, SINGLE, lp->lcore_id,
                    timer_send_checkpoint, lp);
}
