#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "netpaxos.h"
#include "configuration.h"

void usage(char *prog)
{
    printf("Usage: %s configuration-file\n", prog);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv[0]);
        return 0;
    }
    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    dump_configuration(&conf);
    struct paxos_ctx *paxos = make_coordinator(&conf);

    start_paxos(paxos);

    free_paxos_ctx(paxos);
    free_configuration(&conf);

    printf("Exit properly\n");
    return 0;
}