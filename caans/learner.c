#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "netpaxos.h"
#include "netutils.h"
#include "configuration.h"
#include "application_proxy.h"

#include "message.h"

void on_perf(evutil_socket_t fd, short event, void *arg) {
    struct application_ctx *app = arg;
    printf("%4d %8d\n", app->at_second++, app->message_per_second);
    app->message_per_second = 0;
}

void deliver(unsigned int inst, char* val, size_t size, void* arg) {
    struct application_ctx *app = arg;
    app->message_per_second++;
    paxos_log_debug("DELIVERED: %d %s", inst, val);
    int *p = (int *) val;
    int proposer_id = ntohl(*p);
    p = (int *) (val + 4);
    int request_id = ntohl(*p);

    // struct client_request *request = (struct client_request *)(val+8);
    // hexdump_message(request);

    paxos_log_debug("proposer %d, request %d", proposer_id, request_id);
    if (request_id % app->node_count == app->node_id) {
        int n = sendto(app->paxos->sock, val, size, 0,
                        (struct sockaddr *)&app->proxies[proposer_id],
                        sizeof(app->proxies[proposer_id]));
        if (n < 0)
            perror("deliver: sendto error");
    }
}

void usage(char *prog) {
    printf("Usage: %s configuration-file learner_id number_of_learner\n", prog);
}


int main(int argc, char *argv[]) {

    if (argc < 4) {
        usage(argv[0]);
        return 0;
    }

    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    dump_configuration(&conf);

    struct application_ctx *app = malloc( sizeof (struct application_ctx));
    app->node_id = atoi(argv[2]);
    app->node_count = atoi(argv[3]);
    app->at_second = 0;
    app->message_per_second = 0;
    app->proxies = calloc(conf.proposer_count, sizeof(struct sockaddr_in));
    int i;
    for (i = 0; i < conf.proposer_count; i++) {
        ip_to_sockaddr( conf.proposer_address[i],
                    conf.proposer_port[i],
                    &app->proxies[i] );
    }
    struct paxos_ctx *paxos = make_learner(&conf, deliver, app);
    app->paxos = paxos;

    struct event *ev_perf = event_new(paxos->base, -1, EV_TIMEOUT|EV_PERSIST, on_perf, app);
    struct timeval one_second = {1, 0};
    event_add(ev_perf, &one_second);

    event_base_priority_init(paxos->base, 4);
    event_priority_set(ev_perf, 0);

    start_paxos(app->paxos);
    event_free(ev_perf);
    free_paxos_ctx(app->paxos);
    free(app->proxies);
    free(app);
    free_configuration(&conf);

    paxos_log_debug("Exit properly");
    return 0;
}