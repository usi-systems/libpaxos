#ifndef PROXY_H_
#define PROXY_H_

#include <event2/event.h>
#include "uthash.h"
#include "netpaxos.h"
#include "configuration.h"

#define BUFFER_SIZE 64

struct application_ctx {
    int node_id;
    int node_count;
    struct paxos_ctx *paxos;
    int current_request_id;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in *proxies;
    int at_second;
    int message_per_second;
    int enable_db;
    void *db;
    int amount_of_write;
    struct event *ev_read;
};

void start_proxy(struct application_ctx *ctx, int proxy_port);
void clean_proxy(struct application_ctx *ctx);

#endif
