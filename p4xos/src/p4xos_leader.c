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


int proposer_prepare_allocated(struct app_lcore_params_worker *lp,
                                struct paxos_hdr *out);
void check_prepare_timeouts(__rte_unused struct rte_timer *timer, void *arg);
void check_accept_timeouts(__rte_unused struct rte_timer *timer, void *arg);


static int
try_accept(struct app_lcore_params_worker *lp, struct paxos_hdr *paxos_hdr)
{
    paxos_message pm;
    if (proposer_accept(lp->proposer, &pm.u.accept)) {
        paxos_hdr->msgtype = PAXOS_ACCEPT;
        paxos_hdr->inst = rte_cpu_to_be_32(pm.u.accept.iid);
        paxos_hdr->rnd = rte_cpu_to_be_16(pm.u.accept.ballot);
        if (pm.u.accept.value.paxos_value_len) {
            rte_memcpy(&paxos_hdr->value, pm.u.accept.value.paxos_value_val,
                PAXOS_VALUE_SIZE);
        }
        return SUCCESS;
    }
    return NO_ACCEPT_INSTANCE;
}


static inline int promise_handler(struct paxos_hdr *paxos_hdr,
                                  struct app_lcore_params_worker *lp) {
    int vsize = PAXOS_VALUE_SIZE;

    struct paxos_promise promise = {
        .iid = rte_be_to_cpu_32(paxos_hdr->inst),
        .ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
        .value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
        .aid = rte_be_to_cpu_16(paxos_hdr->acptid),
        .value = {vsize, (char *)&paxos_hdr->value},
    };
    RTE_LOG(DEBUG, P4XOS, "Worker %u Received Promise instance %u, ballot %u, "
            "value_ballot %u, aid %u\n", lp->worker_id, promise.iid,
            promise.ballot, promise.value_ballot, promise.aid);

    if (!app.p4xos_conf.leader)
        return DROP_ORIGINAL_PACKET;

    paxos_message pa;
	int preempted = proposer_receive_promise(lp->proposer, &promise, &pa.u.prepare);
	if (preempted) {
        RTE_LOG(DEBUG, P4XOS, "Worker %u Preempted instance %u, ballot %u, ",
            lp->worker_id, pa.u.prepare.iid, pa.u.prepare.ballot);
        paxos_hdr->msgtype = PAXOS_PREPARE;
        paxos_hdr->inst = rte_cpu_to_be_32(pa.u.prepare.iid);
        paxos_hdr->rnd = rte_cpu_to_be_16(pa.u.prepare.ballot);
        return SUCCESS;
    }

    if (lp->has_holes)
    {
        learner_check_holes(lp);
    }
    return try_accept(lp, paxos_hdr);
}

static inline int accepted_handler(struct paxos_hdr *paxos_hdr,
                                   struct app_lcore_params_worker *lp) {
    int vsize = PAXOS_VALUE_SIZE;
    struct paxos_accepted ack = {.iid = rte_be_to_cpu_32(paxos_hdr->inst),
                               .ballot = rte_be_to_cpu_16(paxos_hdr->rnd),
                               .value_ballot = rte_be_to_cpu_16(paxos_hdr->vrnd),
                               .aid = rte_be_to_cpu_16(paxos_hdr->acptid),
                               .value = {vsize, (char *)&paxos_hdr->value}};

    RTE_LOG(DEBUG, P4XOS, "Worker %u, Received Accepted instance %u, ballot %u, aid %u\n",
                  lp->worker_id, ack.iid, ack.ballot, ack.aid);


    if (app.p4xos_conf.leader) {
        proposer_receive_accepted(lp->proposer, &ack);
    }

    learner_receive_accepted(lp->learner, &ack);
    paxos_accepted deliver;
    if (learner_deliver_next(lp->learner, &deliver)) {
        lp->deliver(lp->worker_id, deliver.iid, deliver.value.paxos_value_val,
                deliver.value.paxos_value_len, lp->deliver_arg);

        RTE_LOG(DEBUG, P4XOS, "Worker %u, Delivers instance "
                          "%u,  ballot %u\n", lp->worker_id, deliver.iid, deliver.ballot);
        paxos_hdr->msgtype = PAXOS_CHOSEN;
        paxos_hdr->inst = rte_cpu_to_be_32(deliver.iid);
        paxos_hdr->rnd = rte_cpu_to_be_16(deliver.ballot);
        if (deliver.value.paxos_value_len)
            rte_memcpy(&paxos_hdr->value, deliver.value.paxos_value_val, PAXOS_VALUE_SIZE);
        paxos_accepted_destroy(&deliver);
        return SUCCESS;
    }

    return TO_DROP;
}


static inline int
new_command_handler(struct paxos_hdr *paxos_hdr,
                            struct app_lcore_params_worker *lp) {

    RTE_LOG(DEBUG, P4XOS, "Worker %u: Received NEW_COMMAND\n", lp->worker_id);
    if (lp->has_holes)
        return DROP_ORIGINAL_PACKET;

    proposer_propose(lp->proposer, (const char*)&paxos_hdr->value, PAXOS_VALUE_SIZE);
    if (try_accept(lp, paxos_hdr) == SUCCESS) {
        return SUCCESS;
    } else {
        return proposer_prepare_allocated(lp, paxos_hdr);
    }
}

static inline int learner_chosen_handler(struct paxos_hdr *paxos_hdr,
                                         struct app_lcore_params_worker *lp) {
    int vsize = PAXOS_VALUE_SIZE;

    uint32_t inst = rte_be_to_cpu_32(paxos_hdr->inst);
    if (inst > lp->highest_chosen_inst) {
        lp->highest_chosen_inst = inst;
    }

    RTE_LOG(DEBUG, P4XOS, "Worker %u, Chosen instance %u\n", lp->worker_id, inst);
    lp->deliver(lp->worker_id, inst, (char *)&paxos_hdr->value, vsize, lp->deliver_arg);
    if (app.p4xos_conf.leader)
    {
        return proposer_prepare_allocated(lp, paxos_hdr);
    }
    return SUCCESS;
}


int replica_handler(struct rte_mbuf *pkt_in, void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    paxos_stats(pkt_in, lp);
    int ret = SUCCESS;
    size_t ip_offset = sizeof(struct ether_hdr);
    struct ipv4_hdr *ip =
    rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);

    char src[INET_ADDRSTRLEN];
    char dst[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ip->src_addr), src, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip->dst_addr), dst, INET_ADDRSTRLEN);
    RTE_LOG(DEBUG, P4XOS, "%s => %s\n", src, dst);

    size_t paxos_offset = get_paxos_offset();
    struct paxos_hdr *paxos_hdr =
      rte_pktmbuf_mtod_offset(pkt_in, struct paxos_hdr *, paxos_offset);

    print_paxos_hdr(paxos_hdr);

    uint8_t msgtype = paxos_hdr->msgtype;
    switch (msgtype) {
    case PAXOS_RESET: {
        RTE_LOG(DEBUG, P4XOS, "Worker %u Reset instance %u\n", lp->worker_id,
                                            rte_be_to_cpu_32(paxos_hdr->inst));
        return TO_DROP;
        }
        case NEW_COMMAND: {
            app.p4xos_conf.client.sin_addr.s_addr = ip->src_addr;
            ret = new_command_handler(paxos_hdr, lp);
            if (ret == SUCCESS) {
                set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr,
                    app.p4xos_conf.acceptor_addr.sin_addr.s_addr);
            }
        break;
        }
        case PAXOS_PREPARE: {
            ret = prepare_handler(paxos_hdr, lp);
            if (ret == SUCCESS) {
                set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr, ip->src_addr);
            }
        break;
        }
        case PAXOS_ACCEPT: {
            ret = accept_handler(paxos_hdr, lp);
            if (ret == SUCCESS) {
                set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr,
                            app.p4xos_conf.learner_addr.sin_addr.s_addr);
            }
        break;
        }
        case PAXOS_PROMISE: {
            ret = promise_handler(paxos_hdr, lp);
            if (ret == SUCCESS) {
                set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr,
                    app.p4xos_conf.acceptor_addr.sin_addr.s_addr);
            }
        break;
        }
        case PAXOS_ACCEPTED: {
            ret = accepted_handler(paxos_hdr, lp);
            if (!app.p4xos_conf.respond_to_client) {
                return DROP_ORIGINAL_PACKET;
            }
            set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr,
                app.p4xos_conf.client.sin_addr.s_addr);
        break;
        }
        case PAXOS_CHOSEN: {
            ret = learner_chosen_handler(paxos_hdr, lp);
            if (app.p4xos_conf.leader) {
                set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr,
                    app.p4xos_conf.acceptor_addr.sin_addr.s_addr);
            } else {
                if (!app.p4xos_conf.respond_to_client) {
                    return DROP_ORIGINAL_PACKET;
                }
                set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr,
                    app.p4xos_conf.client.sin_addr.s_addr);
            }
        break;
        }
        case LEARNER_CHECKPOINT: {
            return learner_checkpoint_handler(paxos_hdr, lp);
        break;
        }

        default: {
            RTE_LOG(DEBUG, P4XOS, "No handler for %u. Worker %u Tail pointer %u\n",
            msgtype, paxos_hdr->worker_id,
            rte_be_to_cpu_16(paxos_hdr->log_index));
            return NO_HANDLER;
        }
    }
    size_t data_size = sizeof(struct paxos_hdr);
    prepare_hw_checksum(pkt_in, data_size);
    rte_timer_reset(&lp->recv_timer, app.hz, SINGLE, lp->lcore_id,
                    timer_send_checkpoint, lp);
    return ret;
}


void proposer_preexecute(struct app_lcore_params_worker *lp)
{
    int nb_pkts = app.p4xos_conf.preexec_window - proposer_prepared_count(lp->proposer);
    if (nb_pkts <= 0)
        return;
    RTE_LOG(DEBUG, P4XOS, "Proposer Pre execute %u instances\n", nb_pkts);

    int lcore = app_get_lcore_worker(lp->worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }

    int ret;
    uint16_t port = app.p4xos_conf.tx_port;
    struct rte_mbuf *pkts[nb_pkts];
    paxos_prepare pr;
    ret = rte_pktmbuf_alloc_bulk(app.lcore_params[lcore].pool, pkts, nb_pkts);

    if (ret < 0) {
        RTE_LOG(INFO, P4XOS, "Not enough entries in the mempools\n");
        return;
    }

    int i;
    uint32_t mbuf_idx = lp->mbuf_out[port].n_mbufs;
    for (i = 0; i < nb_pkts; i++) {
        proposer_prepare(lp->proposer, &pr);
        RTE_LOG(DEBUG, P4XOS, "Proposer Pre execute instance %u ballot %u\n", pr.iid, pr.ballot);

        prepare_paxos_message(pkts[i], port, &app.p4xos_conf.mine,
                        &app.p4xos_conf.acceptor_addr,
                        PAXOS_PREPARE, pr.iid, pr.ballot, lp->worker_id,
                        app.p4xos_conf.node_id, 0, 0, NULL, 0);
        lp->mbuf_out[port].array[mbuf_idx++] = pkts[i];
        lp->mbuf_out[port].n_mbufs = mbuf_idx;
    }
    lp->mbuf_out_flush[port] = 1;

    ret = rte_timer_reset(&lp->prepare_timer, app.hz/LEADER_CHECK_TIMEOUT,
                                SINGLE, lcore, check_prepare_timeouts, lp);
    if (ret < 0) {
        RTE_LOG(DEBUG, P4XOS, "timer is in the RUNNING state\n");
    }
}

int proposer_prepare_allocated(struct app_lcore_params_worker *lp, struct paxos_hdr *out)
{
    paxos_prepare pr;
    proposer_prepare(lp->proposer, &pr);
    out->msgtype = PAXOS_PREPARE;
    out->inst = rte_cpu_to_be_32(pr.iid);
    out->rnd = rte_cpu_to_be_16(pr.ballot);
    return SUCCESS;
}


void pre_execute_prepare(__rte_unused struct rte_timer *timer,
                          __rte_unused void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    proposer_preexecute(lp);
}

void send_to_acceptor(struct app_lcore_params_worker *lp, struct paxos_message *pm)
{
    uint16_t port = app.p4xos_conf.tx_port;
    int lcore = app_get_lcore_worker(lp->worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }
    int mbuf_idx = lp->mbuf_out[port].n_mbufs;
    uint8_t msgtype = (uint8_t) pm->type;
    RTE_LOG(DEBUG, P4XOS, "Send msgtype %u inst %u ballot %u\n", msgtype, pm->u.accept.iid, pm->u.accept.ballot);
    struct rte_mbuf *pkt = rte_pktmbuf_alloc(app.lcore_params[lcore].pool);
    if (pkt != NULL) {
        prepare_paxos_message(pkt, port, &app.p4xos_conf.mine,
                        &app.p4xos_conf.acceptor_addr, msgtype, pm->u.accept.iid,
                        pm->u.accept.ballot, lp->worker_id, app.p4xos_conf.node_id,
                        0, 0, pm->u.accept.value.paxos_value_val, pm->u.accept.value.paxos_value_len);
        lp->mbuf_out[port].array[mbuf_idx] = pkt;
        lp->mbuf_out[port].n_mbufs++;
    }
}

void check_prepare_timeouts(struct rte_timer *timer,
                          __rte_unused void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    struct timeout_iterator* iter = proposer_timeout_iterator(lp->proposer);
    paxos_message pm;
    while(timeout_iterator_prepare(iter, &pm.u.prepare)) {
        pm.type = PAXOS_PREPARE;
        send_to_acceptor(lp, &pm);
    }
    timeout_iterator_free(iter);
    int lcore = app_get_lcore_worker(lp->worker_id);
    int ret = rte_timer_reset(timer, app.hz/LEADER_CHECK_TIMEOUT, SINGLE, lcore,
                                check_prepare_timeouts, lp);
    if (ret < 0) {
        RTE_LOG(DEBUG, P4XOS, "timer is in the RUNNING state\n");
    }
}

void check_accept_timeouts(struct rte_timer *timer,
                          __rte_unused void *arg) {
    struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
    struct timeout_iterator* iter = proposer_timeout_iterator(lp->proposer);
    paxos_message pm;
    while(timeout_iterator_accept(iter, &pm.u.accept)) {
        pm.type = PAXOS_ACCEPT;
        send_to_acceptor(lp, &pm);
    }
    timeout_iterator_free(iter);
    int lcore = app_get_lcore_worker(lp->worker_id);
    int ret = rte_timer_reset(timer, app.hz/LEADER_CHECK_TIMEOUT, SINGLE, lcore,
                                check_accept_timeouts, lp);
    if (ret < 0) {
        RTE_LOG(DEBUG, P4XOS, "timer is in the RUNNING state\n");
    }
}
