#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "netpaxos.h"
#include "netutils.h"
#include "configuration.h"
#include "application_proxy.h"
#include "message.h"
#include "leveldb_context.h"

void on_perf(evutil_socket_t fd, short event, void *arg) {
    struct application_ctx *app = arg;
    printf("%4d %8d\n", app->at_second++, app->message_per_second);
    app->message_per_second = 0;
}

void deliver(unsigned int inst, char* val, size_t size, void* arg) {
    struct application_ctx *app = arg;
    app->message_per_second++;
    struct client_request *req = (struct client_request*)val;
    char *retval = (val + sizeof(unsigned) + sizeof(struct sockaddr_in));

    struct command *cmd = (struct command*)(val + sizeof(struct client_request) - 1);
    // printf("content: %s\n", cmd->content);
    // printf("address %s, port %d\n", inet_ntoa(req->cliaddr.sin_addr),ntohs(req->cliaddr.sin_port));
    // printf("%ld.%09ld\n", ts->tv_sec, ts->tv_nsec);
    // int i;
    // printf("MSG\n");
    // for (i = 0; i < size; i++) {
    //     if (i % 16 == 0)
    //         printf("\n");
    //     printf("%02x ", (unsigned char)val[i]);
    // }
    // printf("\n");

    // paxos_log_debug("DELIVERED: %d %s", inst, val);
    // int *p = (int *) val;
    // int proposer_id = ntohl(*p);
    // p = (int *) (val + 4);
    // int request_id = ntohl(*p);
    // if (app->enable_leveldb) {
    //     struct client_request *request = (struct client_request *)(val+8);
    //     char *key = request->content;
    //     if (app->amount_of_write > 0 && request_id % app->amount_of_write == 0) {
    //         // hexdump_message(request);
    //         char *value = request->content + 16;
    //         int res = add_entry(app->leveldb, 0, key, 16, value, 15);
    //         if (res) {
    //             fprintf(stderr, "Add entry failed.\n");
    //         }
    //     } else {
    //         /* check if the value is stored */
    //         char *stored_value = NULL;
    //         size_t vsize = 0;
    //         int res = get_value(app->leveldb, key, 16, &stored_value, &vsize);
    //         if (res) {
    //             fprintf(stderr, "get value failed.\n");
    //         } else {
    //             if (stored_value != NULL) {
    //                 // printf("Stored value %s, size %zu\n", stored_value, vsize);
    //                 free(stored_value);
    //             }
    //         }
    //     }
    // }



    if (cmd->command_id % app->node_count == app->node_id) {
        int n = sendto(app->paxos->sock, retval, content_length(req), 0,
                        (struct sockaddr *)&req->cliaddr,
                        sizeof(req->cliaddr));
        if (n < 0)
            perror("deliver: sendto error");
    }
}

void usage(char *prog) {
    printf("Usage: %s configuration-file learner_id number_of_learner [enable_leveldb]\n", prog);
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
    app->enable_leveldb = 0;

    if (argc > 4) {
        app->enable_leveldb = atoi(argv[4]);
    }

    int percent_write = 5;
    if (argc > 5) {
        percent_write = atoi(argv[5]);
    }
    if (percent_write == 0) {
        app->amount_of_write = 0;
    } else {
        /* Work for less than 50% of write */
        app->amount_of_write = 100 / percent_write;
    }

    app->proxies = calloc(conf.proposer_count, sizeof(struct sockaddr_in));
    int i;
    for (i = 0; i < conf.proposer_count; i++) {
        ip_to_sockaddr( conf.proposer_address[i],
                    conf.proposer_port[i],
                    &app->proxies[i] );
    }
    struct paxos_ctx *paxos = make_learner(&conf, deliver, app);
    app->paxos = paxos;
    app->leveldb = new_leveldb_context();

    struct event *ev_perf = event_new(paxos->base, -1, EV_TIMEOUT|EV_PERSIST, on_perf, app);
    struct timeval one_second = {1, 0};
    event_add(ev_perf, &one_second);

    event_base_priority_init(paxos->base, 4);
    event_priority_set(ev_perf, 0);

    start_paxos(app->paxos);
    event_free(ev_perf);
    free_paxos_ctx(app->paxos);
    free(app->proxies);
    free_leveldb_context(app->leveldb);
    free(app);
    free_configuration(&conf);

    paxos_log_debug("Exit properly");
    return 0;
}