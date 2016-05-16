#include <stdio.h>
#include <stdlib.h>
#include "netpaxos.h"

void deliver(unsigned int inst, char* val, size_t size, void* arg) {
    printf("DELIVERED: %d %s\n", inst, val);
}

void usage(char *prog) {
    printf("Usage: %s configuration-file\n", prog);
}

void populate_configuration(char* config_file, struct netpaxos_configuration *conf) {
    conf->learner_port = 34952;
    conf->acceptor_port = 34951;
    conf->acceptor_address = "127.0.0.1";
    conf->acceptor_count = 1;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        usage(argv[0]);
        return 0;
    }


    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    struct paxos_ctx *ctx = make_learner(&conf, deliver, NULL);
    start_paxos(ctx);
    free_paxos_ctx(ctx);
    printf("Exit properly\n");
    return 0;
}