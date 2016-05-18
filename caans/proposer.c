#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "application_proxy.h"

void usage(char *prog)
{
    printf("Usage: %s configuration-file proposer_id\n", prog);
}

void respond_cb(char *msg, size_t size, void *arg) {

}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        usage(argv[0]);
        return 0;
    }

    int proposer_id = atoi(argv[2]);

    struct application_ctx *app = malloc(sizeof(struct application_ctx));
    app->request_table = NULL;
    app->current_request_id = 0;
    app->proxy_id = proposer_id;

    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    dump_configuration(&conf);
    struct paxos_ctx *paxos = make_proposer(&conf, proposer_id, respond_cb, app);

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