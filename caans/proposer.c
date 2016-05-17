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
    struct bufferevent* bev;
    UT_hash_handle hh;
};

struct application_ctx {
    struct paxos_ctx *paxos;
    int current_request_id;
    struct request_entry *request_table;
};

static void handle_request(struct bufferevent *bev, void *arg)
{
    struct application_ctx *ctx = arg;
    char tmp[32];
    size_t n;
    n = bufferevent_read(bev, tmp, sizeof(tmp));
    if (n <= 0)
        return; /* No data. */
    submit(ctx->paxos, tmp, n);

    struct request_entry *s = malloc(sizeof(struct request_entry));
    s->request_id = ctx->current_request_id++;
    s->bev = bev;
    HASH_ADD_INT(ctx->request_table, request_id, s);
}

static void handle_conn_events(struct bufferevent *bev, short events, void *arg)
{
    if (events & BEV_EVENT_ERROR)
            perror("Error from bufferevent");
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
            bufferevent_free(bev);
    }
}

static void accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *address, int socklen, void *arg)
{
    struct application_ctx *ctx = arg;
    /* We got a new connection! Set up a bufferevent for it. */
    struct event_base *base = evconnlistener_get_base(listener);
    struct bufferevent *bev = bufferevent_socket_new(
            base, fd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, handle_request, NULL, handle_conn_events, ctx);
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
    struct evconnlistener *listener;

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(0);
    server.sin_port = htons(conf->application_port);

    listener = evconnlistener_new_bind(ctx->paxos->base, accept_conn_cb, ctx,
            LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
            (struct sockaddr*)&server, sizeof(server));
    if (!listener) {
            perror("Couldn't create listener");
            return;
    }
    evconnlistener_set_error_cb(listener, accept_error_cb);
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
    app->current_request_id = 0;

    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    dump_configuration(&conf);
    struct paxos_ctx *paxos = make_proposer(&conf);

    app->paxos = paxos;

    start_server(app, &conf);

    start_paxos(app->paxos);
    free_paxos_ctx(app->paxos);
    free(app);
    printf("Exit properly\n");
    return 0;
}