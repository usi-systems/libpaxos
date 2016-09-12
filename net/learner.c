#include "learner.h"
#include "paxos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <signal.h>
#include "netutils.h"
#include "netpaxos.h"
#include "message_pack.h"

#define MAX_GAPS 128

static void
send_paxos_message(struct paxos_ctx* ctx, paxos_message* msg) {
    char buffer[BUFSIZE];
    pack_paxos_message(buffer, msg);
    sendto(ctx->sock, buffer, sizeof(*msg), 0,
            (struct sockaddr*)&ctx->acceptor_sin, sizeof(ctx->acceptor_sin));
}

void check_holes(evutil_socket_t fd, short event, void *arg) {
    struct paxos_ctx *ctx = arg;
    unsigned from_inst;
    unsigned to_inst;
    if (learner_has_holes(ctx->learner_state, &from_inst, &to_inst)) {
        paxos_log_debug("Learner has holes from %d to %d\n", from_inst, to_inst);
        if ((to_inst - from_inst) > MAX_GAPS) {
            int iid;
            for (iid = from_inst; iid < from_inst + MAX_GAPS; iid++) {
                paxos_message out;
                out.type = PAXOS_PREPARE;
                learner_prepare(ctx->learner_state, &out.u.prepare, iid);
                send_paxos_message(ctx, &out);
            }
        }
    }
    event_add(ctx->hole_watcher, &ctx->tv);
}

void on_paxos_accepted(paxos_message *msg, struct paxos_ctx *ctx) {
    learner_receive_accepted(ctx->learner_state, &msg->u.accepted);
    paxos_accepted chosen_value;
    while (learner_deliver_next(ctx->learner_state, &chosen_value)) {
        ctx->deliver(
            chosen_value.iid,
            chosen_value.value.paxos_value_val,
            chosen_value.value.paxos_value_len,
            ctx->deliver_arg);
        paxos_accepted_destroy(&chosen_value);
    }
}

void on_paxos_promise(paxos_message *msg, struct paxos_ctx *ctx) {
    int ret;
    paxos_message out;
    out.type = PAXOS_ACCEPT;
    ret = learner_receive_promise(ctx->learner_state, &msg->u.promise, &out.u.accept);
    if (ret)
        send_paxos_message(ctx, &out);
}

void on_paxos_preempted(paxos_message *msg, struct paxos_ctx *ctx) {
    int ret;
    paxos_message out;
    out.type = PAXOS_PREPARE;
    ret = learner_receive_preempted(ctx->learner_state, &msg->u.preempted, &out.u.prepare);
    if (ret)
        send_paxos_message(ctx, &out);
}

void learner_read_cb(evutil_socket_t fd, short what, void *arg) {
    struct paxos_ctx *ctx = arg;
    if (what&EV_READ) {
        char buffer[BUFSIZE];
        memset(buffer, 0, BUFSIZE);
        struct sockaddr_in remote;
        socklen_t readlen = sizeof(remote);

        int n = recvfrom(fd, buffer, BUFSIZE, 0, (struct sockaddr *)&remote,
            &readlen);
        if (n < 0)
            perror("recvfrom");

        struct paxos_message msg;
        unpack_paxos_message(&msg, buffer);

        switch(msg.type) {
            case PAXOS_ACCEPTED:
                on_paxos_accepted(&msg, ctx);
                break;
            case PAXOS_PROMISE:
                on_paxos_promise(&msg, ctx);
                break;
            case PAXOS_PREEMPTED:
                on_paxos_preempted(&msg, ctx);
            default:
                paxos_log_debug("No handler for message type %d\n", msg.type);
        }
        paxos_message_destroy(&msg);
    }
}


struct paxos_ctx *make_learner(struct netpaxos_configuration *conf,
                                        deliver_function f, void *arg)
{
    struct paxos_ctx *ctx = malloc( sizeof(struct paxos_ctx));
    init_paxos_ctx(ctx);
    ctx->base = event_base_new();

    evutil_socket_t sock = create_server_socket(conf->learner_port);
    evutil_make_socket_nonblocking(sock);
    ctx->sock = sock;

    if (net_ip__is_multicast_ip(conf->learner_address)) {
        subcribe_to_multicast_group(conf->learner_address, sock);
    }

    ip_to_sockaddr(conf->acceptor_address, conf->acceptor_port, &ctx->acceptor_sin);

    ctx->ev_read = event_new(ctx->base, sock, EV_READ|EV_PERSIST,
        learner_read_cb, ctx);
    event_add(ctx->ev_read, NULL);

    ctx->ev_sigint = evsignal_new(ctx->base, SIGINT, handle_signal, ctx);
    evsignal_add(ctx->ev_sigint, NULL);

    ctx->ev_sigterm = evsignal_new(ctx->base, SIGTERM, handle_signal, ctx);
    evsignal_add(ctx->ev_sigterm, NULL);

    ctx->learner_state = learner_new(conf->acceptor_count);
    learner_set_instance_id(ctx->learner_state, 0);
    ctx->deliver = f;
    ctx->deliver_arg = arg;

    ctx->tv.tv_sec = 0;
    ctx->tv.tv_usec = 100000;

    ctx->hole_watcher = evtimer_new(ctx->base, check_holes, ctx);
    event_add(ctx->hole_watcher, &ctx->tv);

    return ctx;
}
