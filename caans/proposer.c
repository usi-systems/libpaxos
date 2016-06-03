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

void respond_cb(char *msg, size_t size, void *arg) {
    struct application_ctx *app = arg;
    int *p = (int *) msg;
    int proposer_id = ntohl(*p);
    p = (int *) (msg + 4);
    int request_id = ntohl(*p);
    paxos_log_debug("proposer %d, request %d", proposer_id, request_id);
    struct request_entry *s;
    HASH_FIND_INT(app->request_table, &request_id, s);
    if (s==NULL) {
        paxos_log_debug("Cannot find the associated buffer event");
    } else {
        paxos_log_debug("Found an entry of request_id %d", s->request_id);
        paxos_log_debug("Address of s->bev %p", s->bev);
        bufferevent_write(s->bev, msg + 8, size - 8);
        HASH_DEL(app->request_table, s);
        free(s);
    }
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
    app->request_table = NULL;
    app->current_request_id = 0;
    app->node_id = proposer_id;

    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    dump_configuration(&conf);
    struct paxos_ctx *paxos = make_proposer(&conf, proposer_id, respond_cb, app);

    app->paxos = paxos;

    start_proxy(app, listen_port);
    start_paxos(app->paxos);

    clean_proxy(app);
    evconnlistener_free(app->listener);
    free_paxos_ctx(app->paxos);
    free(app);
    free_configuration(&conf);

    paxos_log_debug("Exit properly");
    return 0;
}