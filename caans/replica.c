#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
/* Accept new TCP connection */
#include <event2/listener.h>
/* struct bufferevent */
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <errno.h>
#include <string.h>

#include "application_proxy.h"

void deliver(unsigned int inst, char* val, size_t size, void* arg) {
    printf("\n");
    int i;
    for (i = 0; i < size; i++)
        printf("%.2x ", val[i]);
    printf("\n");

    int *raw_int_bytes = (int *) val;
    int request_id = ntohl(*raw_int_bytes);

    paxos_log_debug("Delivered %d", inst);
    paxos_log_debug("request_id: %d", request_id);
    paxos_log_debug("content %s", val+4);
    paxos_log_debug("size %zu", size - 4);
}

void usage(char *prog)
{
    printf("Usage: %s configuration-file port\n", prog);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        usage(argv[0]);
        return 0;
    }
    int proxy_port = atoi(argv[2]);

    struct application_ctx *app = malloc(sizeof(struct application_ctx));

    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    dump_configuration(&conf);
    struct paxos_ctx *paxos = make_replica(&conf, deliver, app);

    app->paxos = paxos;

    start_proxy(app, proxy_port);
    start_paxos(app->paxos);

    free_paxos_ctx(app->paxos);
    free(app);
    free_configuration(&conf);

    paxos_log_debug("Exit properly");
    return 0;
}