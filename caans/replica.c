#include <stdio.h>
#include <stdlib.h>
#include "netpaxos.h"

void usage(char *prog) {
    printf("Usage: %s port\n", prog);
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        usage(argv[0]);
        return 0;
    }

    int port = atoi(argv[1]);
    struct paxos_ctx *ctx = make_learner(port);
    start_paxos(ctx);
    free_paxos_ctx(ctx);
    printf("Exit properly\n");
    return 0;
}