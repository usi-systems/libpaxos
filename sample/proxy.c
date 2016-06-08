/*
 * Copyright (c) 2013-2015, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <paxos.h>
#include <evpaxos.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <event2/event.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <uthash.h>
 /* Accept new TCP connection */
#include <event2/listener.h>
/* struct bufferevent */
#include <event2/buffer.h>
#include <event2/bufferevent.h>
/* application_config */
#include "application_config.h"
#include "application.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 144

struct request_entry {
    int request_id;
    struct bufferevent *bev;
    UT_hash_handle hh;
};

struct proxy_server
{
    int proxy_id;
    int current_request_id;
    int at_second;
    int message_per_second;
    int number_of_learners;
    struct event_base* base;
    struct bufferevent* bev;
    struct bufferevent* *learner_bevs;
    struct event* sigint;
    struct event* sigterm;
    struct evconnlistener *listener;
    struct request_entry *request_table;
};

void handle_request(struct bufferevent *bev, void *arg)
{
    struct proxy_server *proxy = arg;
    struct client_value value;
    value.proxy_id = proxy->proxy_id;
    value.request_id = proxy->current_request_id;
    size_t n = bufferevent_read(bev, value.content, TMP_VALUE_SIZE-1);
    value.content[TMP_VALUE_SIZE-1] = '\0';
    value.size = n;
    paxos_log_debug("submit %d %d %p", value.proxy_id, value.request_id, bev);
    paxos_submit(proxy->bev, (char*)&value, sizeof(value));
    struct request_entry *s = malloc(sizeof(struct request_entry));
    s->request_id = proxy->current_request_id;
    s->bev = bev;
    HASH_ADD_INT(proxy->request_table, request_id, s);
    proxy->current_request_id++;
}

void send_request(struct proxy_server *proxy)
{
    struct client_value value;
    value.proxy_id = proxy->proxy_id;
    value.request_id = proxy->current_request_id;
    value.content[TMP_VALUE_SIZE-1] = '\0';
    value.size = TMP_VALUE_SIZE;
    paxos_log_debug("submit %d %d", value.proxy_id, value.request_id);
    paxos_submit(proxy->bev, (char*)&value, sizeof(value));
}

static void
event_cb(struct bufferevent *bev, short events, void *ctx)
{
        if (events & BEV_EVENT_ERROR)
                perror("Error from bufferevent");
        if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
                bufferevent_free(bev);
        }
}


void accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *address, int socklen, void *arg)
{
    struct event_base *base = evconnlistener_get_base(listener);
    struct bufferevent *bev = bufferevent_socket_new(
            base, fd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, handle_request, NULL, event_cb, arg);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
}

void accept_error_cb(struct evconnlistener *listener, void *arg)
{
    int err = EVUTIL_SOCKET_ERROR();
    fprintf(stderr, "Got an error %d (%s) on the listener. "
        "Shutting down.\n", err, evutil_socket_error_to_string(err));
    struct event_base *base = evconnlistener_get_base(listener);
    event_base_loopexit(base, NULL);
}

void start_proxy(struct proxy_server *ctx, int proxy_port)
{
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(0);
    server.sin_port = htons(proxy_port);

    ctx->listener = evconnlistener_new_bind(ctx->base, accept_conn_cb, ctx,
            LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
            (struct sockaddr*)&server, sizeof(server));
    if (!ctx->listener) {
            perror("Couldn't create listener");
            return;
    }
    evconnlistener_set_error_cb(ctx->listener, accept_error_cb);
}

void handle_sigint(int sig, short ev, void* arg) {
    printf("CAUGHT SIGNINT\n");
    struct event_base* base = arg;
    event_base_loopexit(base, NULL);
}

void handle_sigterm(int sig, short ev, void* arg) {
    struct event_base* base = arg;
    event_base_loopexit(base, NULL);
}

void respond_cb(char *msg, size_t size, void *arg) {
    struct proxy_server *proxy = arg;
    struct client_value *v = (struct client_value *) msg;
    int proxy_id = v->proxy_id;
    int request_id = v->request_id;
    paxos_log_debug("proxy_id %d, request %d", proxy_id, request_id);
    struct request_entry *s;
    HASH_FIND_INT(proxy->request_table, &request_id, s);
    if (s==NULL) {
        paxos_log_debug("Cannot find the associated buffer event");
    } else {
        paxos_log_debug("Found an entry of request_id %d", s->request_id);
        paxos_log_debug("Address of s->bev %p", s->bev);
        bufferevent_write(s->bev, (char*)&v->content, v->size);
        HASH_DEL(proxy->request_table, s);
        free(s);
    }
}

void on_read(struct bufferevent *bev, void *ctx) {
    struct proxy_server* c = ctx;
    char buffer[BUFFER_SIZE];
    size_t n = bufferevent_read(bev, buffer, BUFFER_SIZE);
    respond_cb(buffer, n, c);
}

void on_response(struct bufferevent *bev, void *ctx) {
    struct proxy_server* proxy = ctx;
    char buffer[BUFFER_SIZE];
    bufferevent_read(bev, buffer, BUFFER_SIZE);
    send_request(proxy);
}

void
on_connect(struct bufferevent* bev, short events, void* arg) {
    if (events & BEV_EVENT_CONNECTED) {
        printf("Connected\n");
    }
    if (events & BEV_EVENT_EOF) {
        printf("EOF\n");
    }
    if (events & BEV_EVENT_TIMEOUT) {
        printf("TIMEOUT\n");
    }
    if (events & BEV_EVENT_ERROR) {
        printf("ERROR %s\n", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
    }
    if (events & BEV_EVENT_READING) {
        printf("READING\n");
    }
    if (events & BEV_EVENT_WRITING) {
        printf("WRITING\n");
    }
}


void
on_connect_learner(struct bufferevent* bev, short events, void* arg) {
    if (events & BEV_EVENT_CONNECTED) {
        struct proxy_server *proxy = arg;
        printf("Connected\n");
        bufferevent_write(bev, &proxy->proxy_id, sizeof proxy->proxy_id);
        send_request(proxy);
    }
    if (events & BEV_EVENT_EOF) {
        printf("EOF\n");
    }
    if (events & BEV_EVENT_TIMEOUT) {
        printf("TIMEOUT\n");
    }
    if (events & BEV_EVENT_ERROR) {
        printf("ERROR %s\n", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
    }
    if (events & BEV_EVENT_READING) {
        printf("READING\n");
    }
    if (events & BEV_EVENT_WRITING) {
        printf("WRITING\n");
    }
}
void connect_to_learner(struct proxy_server* proxy, const char* config_file) {
    application_config *config = parse_configuration(config_file);
    proxy->learner_bevs = calloc(config->number_of_learners, sizeof(struct bufferevent*));

    if (config == NULL) {
        printf("Failed to connect to learners\n");
        return;
    }
    int i;
    proxy->number_of_learners = config->number_of_learners;
    for (i = 0; i < proxy->number_of_learners; i++) {
        struct sockaddr_in *learner_sockaddr = address_to_sockaddr_in(&config->learners[i]);
        printf("address %s, port %d\n", inet_ntoa(learner_sockaddr->sin_addr), ntohs(learner_sockaddr->sin_port));

        proxy->learner_bevs[i] = bufferevent_socket_new(proxy->base, -1, BEV_OPT_CLOSE_ON_FREE);
        // bufferevent_setcb(proxy->learner_bevs[i], on_read, NULL, on_connect_learner, proxy);
        bufferevent_setcb(proxy->learner_bevs[i], on_response, NULL, on_connect_learner, proxy); // TEST: without clients
        bufferevent_enable(proxy->learner_bevs[i], EV_READ);
        bufferevent_socket_connect(proxy->learner_bevs[i],
            (struct sockaddr*)learner_sockaddr, sizeof(*learner_sockaddr));
        int flag = 1;
        setsockopt(bufferevent_getfd(proxy->learner_bevs[i]), IPPROTO_TCP,
            TCP_NODELAY, &flag, sizeof(int));
        free(learner_sockaddr);
    }
    free_application_config(config);
}

struct bufferevent*
connect_to_proposer(struct proxy_server* proxy, struct evpaxos_config* conf, int proposer_id) {
    struct bufferevent* bev;
    struct sockaddr_in addr = evpaxos_proposer_address(conf, proposer_id);
    bev = bufferevent_socket_new(proxy->base, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, NULL, NULL, on_connect, proxy);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
    bufferevent_socket_connect(bev, (struct sockaddr*)&addr, sizeof(addr));
    int flag = 1;
    setsockopt(bufferevent_getfd(bev), IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    return bev;
}

void clean_proxy(struct proxy_server* c)
{
    struct request_entry *s, *tmp;
    HASH_ITER(hh, c->request_table, s, tmp) {
        HASH_DEL(c->request_table, s);
        free(s);
    }

    bufferevent_free(c->bev);
    event_free(c->sigterm);
    event_free(c->sigint);
    int i;
    for (i = 0; i < c->number_of_learners; i++) {
        bufferevent_free(c->learner_bevs[i]);
    }
    free(c->learner_bevs);
    // evconnlistener_free(c->listener);
    event_base_free(c->base);
    free(c);
    exit(EXIT_FAILURE);

}

void usage(const char* name) {
    printf("Usage: %s path/to/paxos.conf path/to/app.conf proxy_id [proxy_port]\n", name);
    exit(EXIT_SUCCESS);
}

int
main(int argc, char const *argv[])
{
    int proposer_id = 0;
    int proxy_id = 0;
    // long proxy_port = 6789;
    if (argc < 3) {
        usage(argv[0]);
        return 0;
    }
    proxy_id = atoi(argv[3]);
    if (argc > 4) {
        // proxy_port = strtol(argv[4], NULL, 10);
        if (errno != 0) {
            // proxy_port = 6789;
        }
    }

    struct proxy_server *proxy = malloc(sizeof(struct proxy_server));
    proxy->proxy_id = proxy_id;
    proxy->current_request_id = 0;
    proxy->at_second = 0;
    proxy->message_per_second = 0;
    proxy->request_table = NULL;
    proxy->base = event_base_new();

    struct evpaxos_config* conf = evpaxos_config_read(argv[1]);
    if (conf == NULL) {
        printf("Failed to read config file %s\n", argv[1]);
        clean_proxy(proxy);
        return EXIT_FAILURE;
    }

    proxy->bev = connect_to_proposer(proxy, conf, proposer_id);
    if (proxy->bev == NULL) {
        clean_proxy(proxy);
        return EXIT_FAILURE;
    }
    evpaxos_config_free(conf);

    connect_to_learner(proxy, argv[2]);

    proxy->sigint = evsignal_new(proxy->base, SIGINT, handle_sigint, proxy->base);
    evsignal_add(proxy->sigint, NULL);
    proxy->sigterm = evsignal_new(proxy->base, SIGTERM, handle_sigterm, proxy->base);
    evsignal_add(proxy->sigterm, NULL);
    // start_proxy(proxy, proxy_port);
    event_base_dispatch(proxy->base);
    clean_proxy(proxy);
    evpaxos_config_free(conf);
    return EXIT_SUCCESS;
}
