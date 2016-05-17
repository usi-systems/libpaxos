#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netpaxos.h"
#include "configuration.h"

void send_cb(evutil_socket_t fd, short what, void *arg)
{
    struct paxos_ctx *ctx = arg;
    submit(ctx, "abc", 3);
}


void deliver(unsigned int inst, char* val, size_t size, void* arg) {
    printf("DELIVERED: %d %s\n", inst, val);
}

void usage(char *prog) {
    printf("Usage: %s configuration-file\n", prog);
}


int main(int argc, char *argv[]) {

    if (argc < 2) {
        usage(argv[0]);
        return 0;
    }

    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    dump_configuration(&conf);
    struct paxos_ctx *ctx = make_replica(&conf, deliver, NULL);

    struct event* socket_ready;
    socket_ready = event_new(ctx->base, ctx->sock, EV_WRITE, send_cb, ctx);
    event_add(socket_ready, NULL);

    start_paxos(ctx);
    event_free(socket_ready);
    free_paxos_ctx(ctx);
    free_configuration(&conf);

    printf("Exit properly\n");
    return 0;
}