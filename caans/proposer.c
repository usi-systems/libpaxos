#include <stdio.h>
#include <stdlib.h>
#include "netpaxos.h"

void usage(char *prog)
{
    printf("Usage: %s host port\n", prog);
}

void send_cb(evutil_socket_t fd, short what, void *arg)
{
    struct paxos_ctx *ctx = arg;
    submit(ctx, "abc", 3);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        usage(argv[0]);
        return 0;
    }

    int port = atoi(argv[2]);
    struct paxos_ctx *ctx = make_proposer(argv[1], port);

    struct event* socket_ready;
    socket_ready = event_new(ctx->base, ctx->sock, EV_WRITE, send_cb, ctx);
    event_add(socket_ready, NULL);

    start_paxos(ctx);
    event_free(socket_ready);
    free_paxos_ctx(ctx);

    printf("Exit properly\n");
    return 0;
}