#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "application_proxy.h"
#include "message.h"

void handle_request(struct bufferevent *bev, void *arg)
{

    struct application_ctx *app = arg;

    /* TODO: find cleaner way to serialize application request */
    uint32_t proxy_id = htonl(app->node_id);
    uint32_t net_req_id = htonl(app->current_request_id);
    memcpy(app->buffer, &proxy_id, 4);
    memcpy(app->buffer + 4, &net_req_id, 4);
    size_t n;
    n = bufferevent_read(bev, app->buffer + 8, BUFFER_SIZE - 8);
    size_t total_size = n + 8;

    // struct client_request *request = (struct client_request *)(app->buffer+8);
    // hexdump_message(request);

    if (n <= 0)
        return; /* No data. */

    submit(app->paxos, app->buffer, total_size);

    struct request_entry *s = malloc(sizeof(struct request_entry));
    s->request_id = app->current_request_id;
    s->bev = bev;
    HASH_ADD_INT(app->request_table, request_id, s);
    app->current_request_id++;

}

void handle_conn_events(struct bufferevent *bev, short events, void *arg)
{
    // struct application_ctx *app = arg;
    if (events & BEV_EVENT_ERROR)
            perror("Error from bufferevent");
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        bufferevent_free(bev);
        /*
        unsigned int num_active_connections;
        num_active_connections = HASH_COUNT(app->request_table);
        printf("there are %u open connections\n", num_active_connections);
        */
    }
}

void accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *address, int socklen, void *arg)
{
    /* We got a new connection! Set up a bufferevent for it. */
    struct event_base *base = evconnlistener_get_base(listener);
    struct bufferevent *bev = bufferevent_socket_new(
            base, fd, BEV_OPT_CLOSE_ON_FREE);
    // printf("Original bev %p\n", bev);
    bufferevent_setcb(bev, handle_request, NULL, handle_conn_events, arg);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
}

void accept_error_cb(struct evconnlistener *listener, void *arg)
{
    struct event_base *base = evconnlistener_get_base(listener);
    int err = EVUTIL_SOCKET_ERROR();
    fprintf(stderr, "Got an error %d (%s) on the listener. "
            "Shutting down.\n", err, evutil_socket_error_to_string(err));
    event_base_loopexit(base, NULL);
}


void start_proxy(struct application_ctx *ctx, int proxy_port)
{
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(0);
    server.sin_port = htons(proxy_port);

    ctx->listener = evconnlistener_new_bind(ctx->paxos->base, accept_conn_cb, ctx,
            LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
            (struct sockaddr*)&server, sizeof(server));
    if (!ctx->listener) {
            perror("Couldn't create listener");
            return;
    }
    evconnlistener_set_error_cb(ctx->listener, accept_error_cb);
}

void clean_proxy(struct application_ctx *ctx)
{
    struct request_entry *s, *tmp;
    HASH_ITER(hh, ctx->request_table, s, tmp) {
        // printf("Free request id : %d\n", s->request_id);
        HASH_DEL(ctx->request_table, s);
        free(s);
    }
}
