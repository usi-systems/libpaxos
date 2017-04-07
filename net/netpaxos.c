#include "learner.h"
#include "acceptor.h"
#include "proposer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "netutils.h"
#include "netpaxos.h"

static void sequencer_free(struct sequencer* sq);


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
    ctx->at_second = 0;
    ctx->message_per_second = 0;
    memset(&ctx->proposer_sin, 0, sizeof(struct sockaddr_in));
    memset(&ctx->coordinator_sin, 0, sizeof(struct sockaddr_in));
    memset(&ctx->acceptor_sin, 0, sizeof(struct sockaddr_in));
    ctx->learner_sin = calloc(NUM_OF_THREAD, sizeof(struct sockaddr_in));
    int i;
    for (i= 0; i < NUM_OF_THREAD; i++)
    {
        memset(&ctx->learner_sin[i], 0, sizeof(struct sockaddr_in));
    }
    
    ctx->base = NULL;
    ctx->ev_send = NULL;
    ctx->ev_read = NULL;
    ctx->ev_sigint = NULL;
    ctx->ev_sigterm = NULL;
    ctx->timeout_ev = NULL;
    ctx->hole_watcher = NULL;
    ctx->learner_state = NULL;
    ctx->acceptor_state = NULL;
    ctx->proposer_state = NULL;
    ctx->sequencer = NULL;
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
    if (ctx->sequencer)
        sequencer_free(ctx->sequencer);
    if (ctx->ev_send)
        event_free(ctx->ev_send);
    if (ctx->hole_watcher)
        event_free(ctx->hole_watcher);
    if (ctx->ev_read)
        event_free(ctx->ev_read);
    if (ctx->ev_sigint)
        event_free(ctx->ev_sigint);
    if (ctx->ev_sigterm)
        event_free(ctx->ev_sigterm);
    if (ctx->timeout_ev)
        event_free(ctx->timeout_ev);
    free(ctx->learner_sin);
    event_base_free(ctx->base);
    free(ctx->buffer);
    free(ctx);
}

static void
sequencer_free(struct sequencer* sq)
{
    free(sq);
}

void on_perf(evutil_socket_t fd, short event, void *arg) {
    struct paxos_ctx *ctx = arg;
    printf("%4d %8d\n", ctx->at_second++, ctx->message_per_second);
    ctx->message_per_second = 0;
}
