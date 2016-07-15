#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "application_proxy.h"

void usage(char *prog)
{
    printf("Usage: %s configuration-file proposer_id listen_port\n", prog);
}


int main(int argc, char *argv[])
{
    if (argc < 4) {
        usage(argv[0]);
        return 0;
    }

    int proposer_id = atoi(argv[2]);
    int listen_port = atoi(argv[3]);
    struct application_ctx *app = malloc(sizeof(struct application_ctx));
    app->current_request_id = 0;

    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    dump_configuration(&conf);
    struct paxos_ctx *paxos = make_proposer(&conf, proposer_id, app);

    app->paxos = paxos;

    start_proxy(app, listen_port);
    start_paxos(app->paxos);

    free_paxos_ctx(app->paxos);
    free(app);
    free_configuration(&conf);

    paxos_log_debug("Exit properly");
    return 0;
}