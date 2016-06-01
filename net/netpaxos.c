#include "learner.h"
#include "acceptor.h"
#include "proposer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "netutils.h"
#include "netpaxos.h"


void start_paxos(struct paxos_ctx *ctx)
{
    event_base_dispatch(ctx->base);
}

void handle_signal(evutil_socket_t fd, short what, void *arg)
{
    // printf("Caught SIGINT\n");
    struct paxos_ctx *ctx = arg;
    event_base_loopbreak(ctx->base);
}

void init_paxos_ctx(struct paxos_ctx *ctx)
{
    ctx->sock = 0;
    /* TODO: Remove mock instance */
    ctx->mock_instance = 1;
    ctx->preexec_window = 2;
    memset(&ctx->acceptor_sin, 0, sizeof(struct sockaddr_in));
    memset(&ctx->learner_sin, 0, sizeof(struct sockaddr_in));
    memset(&ctx->proposer_sin, 0, sizeof(struct sockaddr_in));
    memset(&ctx->coordinator_sin, 0, sizeof(struct sockaddr_in));
    ctx->base = NULL;
    ctx->ev_send = NULL;
    ctx->ev_read = NULL;
    ctx->ev_signal = NULL;
    ctx->timeout_ev = NULL;
    ctx->hole_watcher = NULL;
    ctx->learner_state = NULL;
    ctx->acceptor_state = NULL;
    ctx->proposer_state = NULL;
    ctx->buffer = malloc(BUFSIZE);
}

void free_paxos_ctx(struct paxos_ctx *ctx)
{
    if (ctx->learner_state)
        learner_free(ctx->learner_state);
    if (ctx->acceptor_state)
        acceptor_free(ctx->acceptor_state);
    if (ctx->proposer_state)
        proposer_free(ctx->proposer_state);
    if (ctx->ev_send)
        event_free(ctx->ev_send);
    if (ctx->hole_watcher)
        event_free(ctx->hole_watcher);
    if (ctx->ev_read)
        event_free(ctx->ev_read);
    if (ctx->ev_signal)
        event_free(ctx->ev_signal);
    if (ctx->timeout_ev)
        event_free(ctx->timeout_ev);
    event_base_free(ctx->base);
    free(ctx->buffer);
    free(ctx);
}
