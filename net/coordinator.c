#include "paxos.h"
#include "proposer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <signal.h>
#include "netutils.h"
#include "netpaxos.h"
#include "message_pack.h"

void coordinator_preexecute(struct paxos_ctx* ctx);
void try_accept(struct paxos_ctx *ctx);

void send_paxos_message(struct paxos_ctx *ctx, struct paxos_message *msg) {
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    pack_paxos_message(buffer, msg);
    size_t msg_len = sizeof(struct paxos_message);
    int n = sendto(ctx->sock, buffer, msg_len, 0,
        (struct sockaddr *)&ctx->acceptor_sin, sizeof(ctx->acceptor_sin));
    if (n < 0)
        perror("Sendto:");
}

void coordinator_handle_proposal(struct paxos_ctx *ctx, struct paxos_message *msg)
{
    struct paxos_value* value = &msg->u.accept.value;
    proposer_propose(ctx->proposer_state,
        value->paxos_value_val,
        value->paxos_value_len);
    try_accept(ctx);
}

void coordinator_handle_accepted(struct paxos_ctx *ctx, paxos_message* msg)
{
    paxos_accepted* acc = &msg->u.accepted;
    if (proposer_receive_accepted(ctx->proposer_state, acc))
        try_accept(ctx);
}


void try_accept(struct paxos_ctx *ctx)
{
    paxos_accept accept;
    while (proposer_accept(ctx->proposer_state, &accept)) {
        struct paxos_message msg = {
            .type = PAXOS_ACCEPT,
            .u.accept = accept
        };
        printf("Send ACCEPT for instance %d\n", msg.u.accept.iid);

        send_paxos_message(ctx, &msg);
    }
    coordinator_preexecute(ctx);
}


void coordinator_handle_promise(struct paxos_ctx *ctx, struct paxos_message *msg)
{
    paxos_prepare prepare;
    paxos_promise* pro = &msg->u.promise;
    int preempted = proposer_receive_promise(ctx->proposer_state, pro, &prepare);
    if (preempted) {
        printf("Prepare instance %d ballot %d\n", prepare.iid, prepare.ballot);
        struct paxos_message retry_msg = {
            .type = PAXOS_PREPARE,
            .u.prepare = prepare
        };
        send_paxos_message(ctx, &retry_msg);
    }
    try_accept(ctx);
}

void coordinator_read(evutil_socket_t fd, short what, void *arg)
{
    struct paxos_ctx *ctx = arg;
    if (what&EV_READ) {
        memset(ctx->buffer, 0, BUFSIZE);
        struct sockaddr_in remote;
        socklen_t len = sizeof(remote);
        int n = recvfrom(fd, ctx->buffer, BUFSIZE, 0,
                            (struct sockaddr *)&remote, &len);
        if (n < 0)
            perror("recvfrom");

        struct paxos_message msg;
        unpack_paxos_message(&msg, ctx->buffer);

        if (msg.type == PAXOS_PROMISE) {
            printf("Received promise for instance %d ballot %d value len %d ",
                msg.u.promise.iid,
                msg.u.promise.ballot,
                msg.u.promise.value.paxos_value_len);
            coordinator_handle_promise(ctx, &msg);
        }
        else if (msg.type == PAXOS_ACCEPT) {
            coordinator_handle_proposal(ctx, &msg);
        }
        else if (msg.type == PAXOS_ACCEPTED) {
            coordinator_handle_accepted(ctx, &msg);
        }
        paxos_message_destroy(&msg);
    }
}

static void
evproposer_check_timeouts(evutil_socket_t fd, short event, void *arg)
{
    struct paxos_ctx *ctx = arg;
    struct timeout_iterator* iter = proposer_timeout_iterator(ctx->proposer_state);

    paxos_prepare pr;
    while (timeout_iterator_prepare(iter, &pr)) {
        printf("Instance %d timed out in phase 1.\n", pr.iid);
        struct paxos_message msg = {
            .type = PAXOS_PREPARE,
            .u.prepare = pr
        };
        send_paxos_message(ctx, &msg);
    }

    paxos_accept ar;
    while (timeout_iterator_accept(iter, &ar)) {
        printf("Instance %d timed out in phase 2.\n", ar.iid);
        struct paxos_message msg = {
            .type = PAXOS_ACCEPT,
            .u.accept = ar
        };
        send_paxos_message(ctx, &msg);
    }

    timeout_iterator_free(iter);
    event_add(ctx->timeout_ev, &ctx->tv);
}

void coordinator_preexecute(struct paxos_ctx* ctx)
{
    int i;
    paxos_prepare pr;
    int count = ctx->preexec_window - proposer_prepared_count(ctx->proposer_state);
    if (count <= 0) return;
    for (i = 0; i < count; i++) {
        proposer_prepare(ctx->proposer_state, &pr);
        printf("Prepare for instance %d, round %d\n",
        pr.iid,
        pr.ballot);
        struct paxos_message msg = {
            .type = PAXOS_PREPARE,
            .u.prepare = pr
        };
        send_paxos_message(ctx, &msg);
    }
    printf("Opened %d new instances\n", count);
}


struct paxos_ctx *make_coordinator(struct netpaxos_configuration *conf)
{
    struct paxos_ctx *ctx = malloc( sizeof(struct paxos_ctx));
    init_paxos_ctx(ctx);

    ctx->my_id = 0;
    ctx->proposer_state = proposer_new(ctx->my_id, conf->acceptor_count);

    ctx->base = event_base_new();

    evutil_socket_t sock = create_server_socket(conf->coordinator_port);
    evutil_make_socket_nonblocking(sock);
    ctx->sock = sock;

    ctx->tv.tv_sec = 1;
    ctx->tv.tv_usec = 0;
    ctx->timeout_ev = evtimer_new(ctx->base, evproposer_check_timeouts, ctx);
    event_add(ctx->timeout_ev, &ctx->tv);


    ip_to_sockaddr(conf->acceptor_address, conf->acceptor_port, &ctx->acceptor_sin);

    ctx->ev_read = event_new(ctx->base, sock, EV_READ|EV_PERSIST,
        coordinator_read, ctx);
    event_add(ctx->ev_read, NULL);

    ctx->ev_signal = evsignal_new(ctx->base, SIGINT, handle_signal, ctx);
    evsignal_add(ctx->ev_signal, NULL);

    coordinator_preexecute(ctx);

    return ctx;
}
