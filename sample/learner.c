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


#include <stdlib.h>
#include <stdio.h>
#include <evpaxos.h>
#include <signal.h>
#include <string.h>

/* Accept new TCP connection */
#include <event2/listener.h>
#include <event2/event.h>
#include <errno.h>
#include "net_utils.h"
#include "application.h"
#include "application_config.h"
#include "leveldb_context.h"
#include "paxos.h"
#include "uthash.h"

struct proxy_entry {
    int proxy_id;
    struct bufferevent *bev;
    UT_hash_handle hh;
};

struct stats
{
    int delivered_count;
    int at_second;
};

struct application_ctx
{
    struct leveldb_ctx* leveldb;
    struct event_base *base;
    struct event* stats_ev;
    struct timeval stats_interval;
    struct stats stats;
    struct proxy_entry *proxy_table;
};

struct application_ctx* new_application() {
    struct application_ctx *ctx = malloc(sizeof (struct application_ctx));
    return ctx;
}


void init_application(struct application_ctx *ctx) {
    ctx->leveldb = new_leveldb_context();
}


void free_application(struct application_ctx *ctx) {
    free_leveldb_context(ctx->leveldb);
    event_free(ctx->stats_ev);
    event_base_free(ctx->base);
    free(ctx);
}

static void handle_sigint(int sig, short ev, void* arg) {
	struct event_base* base = arg;
	event_base_loopexit(base, NULL);
}

static void on_stats(evutil_socket_t fd, short event, void *arg) {
    struct application_ctx* ctx = arg;
    fprintf(stdout, "%d %d\n", ctx->stats.at_second, ctx->stats.delivered_count);
    ctx->stats.delivered_count = 0;
    ctx->stats.at_second++;
}

static void deliver(unsigned iid, char* value, size_t size, void* arg) {
	struct application_ctx *ctx = arg;
	struct client_value* val = (struct client_value*)value;
	if (val->application_type == LEVELDB) {
		struct leveldb_request *req = (struct leveldb_request *) &val->content;
		switch(req->op) {
			case PUT: {
				 // paxos_log_debug("PUT: key %s: %zu, value %s: %zu", req->key, req->ksize, req->value, req->vsize);
				 add_entry(ctx->leveldb, false, req->key, req->ksize, req->value, req->vsize);
				 break;
            }
			case GET: {
				 char *value;
				 size_t vsize = 0;
				 get_value(ctx->leveldb, req->key, req->ksize, &value, &vsize);
				 // paxos_log_debug("GET: key %s: %zu, RETURN: %s %zu", req->key, req->ksize, value, vsize);
				 if (value)
				     free(value);
				 break;
    		}
        }
	}
    ctx->stats.delivered_count++;
    int proxy_id = val->proxy_id;
    struct proxy_entry *s;
    HASH_FIND_INT(ctx->proxy_table, &proxy_id, s);
    if (s==NULL) {
        paxos_log_debug("Cannot find the associated buffer event");
    } else {
        paxos_log_debug("Found an entry of proxy %d", s->proxy_id);
        paxos_log_debug("Address of s->bev %p", s->bev);
        bufferevent_write(s->bev,(char*)val, size);
    }
}


static void
handle_conn_events(struct bufferevent *bev, short events, void *arg)
{
    if (events & BEV_EVENT_ERROR)
            perror("Error from bufferevent");
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
            bufferevent_free(bev);
    }
}

void handle_request(struct bufferevent *bev, void *arg)
{
    struct application_ctx *ctx = arg;
    int proxy_id;
    bufferevent_read(bev, &proxy_id, sizeof proxy_id);
    struct proxy_entry *s = malloc(sizeof(struct proxy_entry));
    s->proxy_id = proxy_id;
    s->bev = bev;
    HASH_ADD_INT(ctx->proxy_table, proxy_id, s);
}

static void
accept_conn_cb(struct evconnlistener *listener,
    evutil_socket_t fd, struct sockaddr *address, int socklen,
    void *arg)
{
        struct application_ctx *ctx = arg;
        struct event_base *base = evconnlistener_get_base(listener);
        struct bufferevent *bev = bufferevent_socket_new(
                base, fd, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(bev, handle_request, NULL, handle_conn_events, ctx);
        bufferevent_enable(bev, EV_READ|EV_WRITE);
}

static void
accept_error_cb(struct evconnlistener *listener, void *ctx)
{
        struct event_base *base = evconnlistener_get_base(listener);
        int err = EVUTIL_SOCKET_ERROR();
        fprintf(stderr, "Got an error %d (%s) on the listener. "
                "Shutting down.\n", err, evutil_socket_error_to_string(err));

        event_base_loopexit(base, NULL);
}

static void start_server(struct application_ctx *ctx, const char* argv[]) {
    struct evconnlistener *listener;
    int learner_id = atoi(argv[3]);
    application_config *conf = parse_configuration(argv[2]);
    struct sockaddr_in *learner_sin = address_to_sockaddr_in(&conf->learners[learner_id]);

    listener = evconnlistener_new_bind(ctx->base, accept_conn_cb, ctx,
            LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
            (struct sockaddr*)learner_sin, sizeof(*learner_sin));
    if (!listener) {
            perror("Couldn't create listener");
            return;
    }
    evconnlistener_set_error_cb(listener, accept_error_cb);

    free(learner_sin);
    free_application_config(conf);
}

static void start_learner(const char* argv[]) {
	struct event* sig;
	struct evlearner* lea;

    struct application_ctx *ctx = new_application();
    init_application(ctx);

	ctx->base = event_base_new();
    memset(&ctx->stats, 0, sizeof(struct stats));
    struct timeval one_second = {1, 0};
    ctx->stats_ev = event_new(ctx->base, -1, EV_TIMEOUT|EV_PERSIST, on_stats, ctx);
    event_add(ctx->stats_ev, &one_second);

	lea = evlearner_init(argv[1], deliver, ctx, ctx->base);
	if (lea == NULL) {
		printf("Could not start the learner!\n");
		exit(1);
	}
	
	sig = evsignal_new(ctx->base, SIGINT|SIGTERM, handle_sigint, ctx->base);
	evsignal_add(sig, NULL);

	signal(SIGPIPE, SIG_IGN);

    start_server(ctx, argv);

	event_base_dispatch(ctx->base);

	event_free(sig);
	evlearner_free(lea);
	free_application(ctx);
}


int main(int argc, char const *argv[]) {
    if (argc != 4) {
        printf("Usage: %s path/to/paxos.conf path/to/application.conf learner_id\n", argv[0]);
        exit(1);
    }
    start_learner(argv);
	return 0;
}
