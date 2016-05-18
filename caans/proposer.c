#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "application_proxy.h"

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

    struct application_ctx *app = malloc(sizeof(struct application_ctx));
    app->request_table = NULL;
    app->current_request_id = 0;

    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    dump_configuration(&conf);
    struct paxos_ctx *paxos = make_proposer(&conf);

    app->paxos = paxos;

    start_proxy(app, &conf);
    start_paxos(app->paxos);

    clean_proxy(app);
    evconnlistener_free(app->listener);
    free_paxos_ctx(app->paxos);
    free(app);
    free_configuration(&conf);

    printf("Exit properly\n");
    return 0;
}