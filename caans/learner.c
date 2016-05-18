#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netpaxos.h"
#include "configuration.h"


void deliver(unsigned int inst, char* val, size_t size, void* arg) {
    printf("DELIVERED: %d %s\n", inst, val);
    // struct application_ctx *app = arg;
    int *p = (int *) val;
    int proposer_id = ntohl(*p);
    p = (int *) (val + 4);
    int request_id = ntohl(*p);
    printf("proposer %d, request %d\n", proposer_id, request_id);
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
    struct paxos_ctx *ctx = make_learner(&conf, deliver, NULL);
    start_paxos(ctx);
    free_paxos_ctx(ctx);
    free_configuration(&conf);
    printf("Exit properly\n");
    return 0;
}