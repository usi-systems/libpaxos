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
#include "netpaxos.h"
#include "configuration.h"
#include "uthash.h"


struct request_entry {
    int request_id;
    struct bufferevent *bev;
    UT_hash_handle hh;
};

struct application_ctx {
    struct paxos_ctx *paxos;
    struct evconnlistener *listener;
    int current_request_id;
    struct request_entry *request_table;
    struct bufferevent *tmpbev;
};

static void handle_request(struct bufferevent *bev, void *arg)
{
    struct application_ctx *app = arg;
    char tmp[32];

    int *inst = (int *)tmp;
    *inst = htonl(app->current_request_id);
    size_t n;
    n = bufferevent_read(bev, tmp + 4, sizeof(tmp));
    tmp[n+4] = '\0';
    if (n <= 0)
        return; /* No data. */
    submit(app->paxos, tmp, n + 4);

    struct request_entry *s = malloc(sizeof(struct request_entry));
    s->request_id = app->current_request_id;
    s->bev = bev;
    HASH_ADD_INT(app->request_table, request_id, s);
    app->current_request_id++;
}

static void handle_conn_events(struct bufferevent *bev, short events, void *arg)
{
    struct application_ctx *app = arg;
    if (events & BEV_EVENT_ERROR)
            perror("Error from bufferevent");
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        bufferevent_free(bev);
        unsigned int num_active_connections;
        num_active_connections = HASH_COUNT(app->request_table);
        printf("there are %u open connections\n", num_active_connections);
    }
}

static void accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *address, int socklen, void *arg)
{
    /* We got a new connection! Set up a bufferevent for it. */
    struct event_base *base = evconnlistener_get_base(listener);
    struct bufferevent *bev = bufferevent_socket_new(
            base, fd, BEV_OPT_CLOSE_ON_FREE);
    printf("Original bev %p\n", bev);
    bufferevent_setcb(bev, handle_request, NULL, handle_conn_events, arg);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
}

static void accept_error_cb(struct evconnlistener *listener, void *arg)
{
    struct event_base *base = evconnlistener_get_base(listener);
    int err = EVUTIL_SOCKET_ERROR();
    fprintf(stderr, "Got an error %d (%s) on the listener. "
            "Shutting down.\n", err, evutil_socket_error_to_string(err));
    event_base_loopexit(base, NULL);
}


static void start_server(struct application_ctx *ctx, struct netpaxos_configuration *conf)
{
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(0);
    server.sin_port = htons(conf->application_port);

    ctx->listener = evconnlistener_new_bind(ctx->paxos->base, accept_conn_cb, ctx,
            LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
            (struct sockaddr*)&server, sizeof(server));
    if (!ctx->listener) {
            perror("Couldn't create listener");
            return;
    }
    evconnlistener_set_error_cb(ctx->listener, accept_error_cb);
}

void deliver(unsigned int inst, char* val, size_t size, void* arg) {
    printf("\n");
    int i;
    for (i = 0; i < size; i++)
        printf("%.2x ", val[i]);
    printf("\n");

    struct application_ctx *app = arg;
    int *raw_int_bytes = (int *) val;
    int request_id = ntohl(*raw_int_bytes);

    printf("Delivered %d\n", inst);
    printf("request_id: %d\n", request_id);
    // printf("content %s\n", val+4);
    // printf("size %zu\n", size - 4);
    struct request_entry *s;
    HASH_FIND_INT(app->request_table, &request_id, s);
    if (s==NULL) {
        printf("Cannot find the associated buffer event\n");
    } else {
        printf("Found an entry of request_id %d\n", s->request_id);
        printf("Address of s->bev %p\n", s->bev);
        bufferevent_write(s->bev, val + 4, size - 4);
        HASH_DEL(app->request_table, s);
        free(s);
    }
}

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
    /* ADD NULL to entry the first entry to prevent damage when delete entry */
    struct request_entry *s = malloc(sizeof(struct request_entry));
    int req_id = 0;
    s->request_id = req_id;
    s->bev = NULL;
    HASH_ADD_INT(app->request_table, request_id, s);

    app->current_request_id = 10;

    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    dump_configuration(&conf);
    struct paxos_ctx *paxos = make_replica(&conf, deliver, app);

    app->paxos = paxos;

    start_server(app, &conf);
    start_paxos(app->paxos);

    HASH_FIND_INT(app->request_table, &req_id, s);
    HASH_DEL(app->request_table, s);
    free(s);

    evconnlistener_free(app->listener);
    free_paxos_ctx(app->paxos);
    free(app);
    free_configuration(&conf);

    printf("Exit properly\n");
    return 0;
}