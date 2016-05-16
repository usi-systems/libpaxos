#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "netutils.h"
#include "netpaxos.h"


void start_paxos(struct paxos_ctx *ctx) {
    event_base_dispatch(ctx->base);
}

void handle_signal(evutil_socket_t fd, short what, void *arg) {
    printf("Caught SIGINT\n");
    struct paxos_ctx *ctx = arg;
    event_base_loopbreak(ctx->base);
}

void init_paxos_ctx(struct paxos_ctx *ctx) {
    ctx->sock = 0;
    memset(&ctx->dest, 0, sizeof(struct sockaddr_in));
    ctx->base = NULL;
    ctx->ev_send = NULL;
    ctx->ev_read = NULL;
    ctx->ev_signal = NULL;
}

void free_paxos_ctx(struct paxos_ctx *ctx) {
    if (ctx->ev_send)
        event_free(ctx->ev_send);

    event_free(ctx->ev_read);
    event_free(ctx->ev_signal);
    event_base_free(ctx->base);
    free(ctx);
}
