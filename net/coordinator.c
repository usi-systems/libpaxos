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

void send_paxos_message(struct paxos_ctx *ctx, struct paxos_message *msg) {
    pack_paxos_message(ctx->buffer, msg);
    size_t msg_len = sizeof(struct paxos_message);
    int n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
        (struct sockaddr *)&ctx->acceptor_sin, sizeof(ctx->acceptor_sin));
    if (n < 0)
        perror("Sendto:");
}

void coordinator_handle_proposal(struct paxos_ctx *ctx, struct paxos_message *msg)
{
    msg->u.accept.iid = ctx->mock_instance++;
    send_paxos_message(ctx, msg);
}

void received_fast_accept(evutil_socket_t fd, short what, void *arg)
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

        if (msg.type == PAXOS_ACCEPT) {
            coordinator_handle_proposal(ctx, &msg);
        }
        paxos_message_destroy(&msg);
    }
}

static void
evproposer_check_timeouts(evutil_socket_t fd, short event, void *arg)
{
    struct paxos_ctx *ctx = arg;
    struct timeout_iterator* iter = proposer_timeout_iterator(ctx->proposer_state);

    paxos_prepare pr
    while (timeout_iterator_prepare(iter, &pr)) {
        paxos_log_info("Instance %d timed out in phase 1.", pr.iid);
        struct paxos_message msg = {
            .type = PAXOS_PREPARE,
            .u.prepare = pr
        };
        send_paxos_message(ctx, &msg);
    }

    paxos_accept ar;
    while (timeout_iterator_accept(iter, &ar)) {
        paxos_log_info("Instance %d timed out in phase 2.", ar.iid);
        struct paxos_message msg = {
            .type = PAXOS_ACCEPT,
            .u.accept = ar
        };
        send_paxos_message(ctx, &msg);
    }

    timeout_iterator_free(iter);
    event_add(ctx->timeout_ev, &ctx->tv);
}

static void
coordinator_preexecute(struct paxos_ctx* ctx)
{
    int i;
    paxos_prepare pr;
    int count = ctx->preexec_window - proposer_prepared_count(ctx->proposer_state);
    if (count <= 0) return;
    for (i = 0; i < count; i++) {
        proposer_prepare(ctx->proposer_state, &pr);
        struct paxos_message msg = {
            .type = PAXOS_PREPARE,
            .u.prepare = pr
        };
        send_paxos_message(ctx, &msg);
    }
    paxos_log_debug("Opened %d new instances", count);
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
        received_fast_accept, ctx);
    event_add(ctx->ev_read, NULL);

    ctx->ev_signal = evsignal_new(ctx->base, SIGINT, handle_signal, ctx);
    evsignal_add(ctx->ev_signal, NULL);

    return ctx;
}
