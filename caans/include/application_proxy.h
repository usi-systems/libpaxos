#ifndef PROXY_H_
#define PROXY_H_

/* Accept new TCP connection */
#include <event2/listener.h>
/* struct bufferevent */
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include "uthash.h"
#include "netpaxos.h"
#include "configuration.h"
#include "leveldb_context.h"

#define BUFFER_SIZE 64

struct request_entry {
    int request_id;
    struct bufferevent *bev;
    UT_hash_handle hh;
};


struct application_ctx {
    int node_id;
    int node_count;
    struct paxos_ctx *paxos;
    struct evconnlistener *listener;
    int current_request_id;
    struct request_entry *request_table;
    struct bufferevent *tmpbev;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in *proxies;
    int at_second;
    int message_per_second;
    int enable_leveldb;
    struct leveldb_ctx *leveldb;
    int amount_of_write;
};

void start_proxy(struct application_ctx *ctx, int proxy_port);
void clean_proxy(struct application_ctx *ctx);

#endif