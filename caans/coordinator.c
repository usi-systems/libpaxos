#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "netpaxos.h"
#include "configuration.h"

void usage(char *prog)
{
    printf("Usage: %s configuration-file [id]\n", prog);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv[0]);
        return 0;
    }
    int my_id = 0;
    if (argc > 2) {
        my_id = atoi(argv[2]);
    }

    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    dump_configuration(&conf);
    struct paxos_ctx *paxos = make_coordinator(&conf, my_id);

    start_paxos(paxos);

    free_paxos_ctx(paxos);
    free_configuration(&conf);

    paxos_log_debug("Exit properly");
    return 0;
}