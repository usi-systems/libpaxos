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
#include "rocksdb_context.h"

void on_perf(evutil_socket_t fd, short event, void *arg) {
    struct application_ctx *app = arg;
    printf("%4d %8d\n", app->at_second++, app->message_per_second);
    app->message_per_second = 0;
}

void deliver(unsigned int inst, char* val, size_t size, void* arg) {
    struct application_ctx *app = arg;
    app->message_per_second++;
    if (size <= 0)
        return;
    struct client_request *req = (struct client_request*)val;

    struct command *cmd = (struct command*)(val + sizeof(struct client_request) - 1);
    if (app->enable_db) {
        char *key = cmd->content;
        if (cmd->op == SET) {
            char *value = cmd->content + 16;
            paxos_log_debug("SET(%s, %s)", key, value);
            int res = add_entry(app->db, 0, key, 16, value, 16);
            if (res) {
                fprintf(stderr, "Add entry failed.\n");
            }
        }
        else if (cmd->op == GET) {
            /* check if the value is stored */
            char *stored_value = NULL;
            size_t vsize = 0;
            int res = get_value(app->db, key, 16, &stored_value, &vsize);
            if (res) {
                fprintf(stderr, "get value failed.\n");
            }
            else {
                if (stored_value != NULL) {
                    paxos_log_debug("Stored value %s, size %zu", stored_value, vsize);
                    free(stored_value);
                }
            }
        }
    }
    /* Skip command ID and client address */
    char *retval = (val + sizeof(uint16_t) + sizeof(struct sockaddr_in));

    /* TEST only the first learner responds */
    // if (cmd->command_id % app->node_count == app->node_id) {
    if (app->node_id == 0) {
        // print_addr(&req->cliaddr);
        int n = sendto(app->paxos->sock, retval, content_length(req), 0,
                        (struct sockaddr *)&req->cliaddr,
                        sizeof(req->cliaddr));
        if (n < 0)
            perror("deliver: sendto error");
    }
}

void usage(char *prog) {
    printf("Usage: %s configuration-file learner_id number_of_learner [enable_rocksdb]\n", prog);
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
    app->enable_db = 0;

    if (argc > 4) {
        app->enable_db = atoi(argv[4]);
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

    if (app->enable_db) {
        app->db = (struct rocksdb_context*) new_rocksdb_context();
    }

    struct event *ev_perf = event_new(paxos->base, -1, EV_TIMEOUT|EV_PERSIST, on_perf, app);
    struct timeval one_second = {1, 0};
    event_add(ev_perf, &one_second);

    event_base_priority_init(paxos->base, 4);
    event_priority_set(ev_perf, 0);

    start_paxos(app->paxos);
    event_free(ev_perf);
    free_paxos_ctx(app->paxos);
    free(app->proxies);
    if (app->enable_db) {
        free_db_context((struct rocksdb_ctx*)app->db);
    }
    free(app);
    free_configuration(&conf);

    paxos_log_debug("Exit properly");
    return 0;
}
