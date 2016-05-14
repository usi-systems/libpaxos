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
#include <errno.h>
#include "net_utils.h"
#include "application.h"
#include "application_config.h"
#include "leveldb_context.h"


struct stats
{
    int delivered_count;
    size_t delivered_bytes;
};

struct application_ctx
{
    struct leveldb_ctx* leveldb;
    struct event_base *base;
    struct event* stats_ev;
    struct timeval stats_interval;
    struct stats stats;
    struct bufferevent *client_bev;
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
	printf("Caught signal %d\n", sig);
	event_base_loopexit(base, NULL);
}

static void on_stats(evutil_socket_t fd, short event, void *arg) {
    struct application_ctx* ctx = arg;
    fprintf(stdout, "%d value/sec\n", ctx->stats.delivered_count);
    memset(&ctx->stats, 0, sizeof(struct stats));
    event_add(ctx->stats_ev, &ctx->stats_interval);
}

static void deliver(unsigned iid, char* value, size_t size, void* arg) {
	struct application_ctx *ctx = arg;
	struct client_value* val = (struct client_value*)value;
	if (val->application_type == LEVELDB) {
		struct leveldb_request *req = (struct leveldb_request *) &val->content;
		switch(req->op) {
			case PUT: {
				 printf("PUT: key %s: %zu, value %s: %zu\n", req->key, req->ksize, req->value, req->vsize);
				 add_entry(ctx->leveldb, false, req->key, req->ksize, req->value, req->vsize);
				 break;
            }
			case GET: {
				 char *value;
				 size_t vsize = 0;
				 get_value(ctx->leveldb, req->key, req->ksize, &value, &vsize);
				 printf("GET: key %s: %zu, RETURN: %s %zu\n", req->key, req->ksize, value, vsize);
				 if (value)
				     free(value);
				 break;
    		}
        }
	} else if (val->application_type == SIMPLY_ECHO) {
        /*
		struct echo_request *req = (struct echo_request *)&val->content;
        printf("%ld.%06ld [%.32s] %ld bytes\n", val->depart_ts.tv_sec,
            val->depart_ts.tv_usec, req->value, (long)val->size);
        */
	}
    ctx->stats.delivered_count++;
    if (ctx->client_bev)
        bufferevent_write(ctx->client_bev, (char*)val, size);
}


static void
handle_conn_events(struct bufferevent *bev, short events, void *arg)
{
    struct application_ctx *ctx = arg;
    if (events & BEV_EVENT_ERROR)
            perror("Error from bufferevent");
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
            bufferevent_free(bev);
            ctx->client_bev = NULL;
    }
}

static void
accept_conn_cb(struct evconnlistener *listener,
    evutil_socket_t fd, struct sockaddr *address, int socklen,
    void *arg)
{
        struct application_ctx *ctx = arg;
        /* We got a new connection! Set up a bufferevent for it. */
        printf("We got a new connection! Set up a bufferevent for it.\n");
        struct event_base *base = evconnlistener_get_base(listener);
        struct bufferevent *bev = bufferevent_socket_new(
                base, fd, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(bev, NULL, NULL, handle_conn_events, ctx);
        bufferevent_enable(bev, EV_READ|EV_WRITE);
        ctx->client_bev = bev;
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
    ctx->stats_interval = (struct timeval){1, 0};
    ctx->stats_ev = evtimer_new(ctx->base, on_stats, ctx);
    event_add(ctx->stats_ev, &ctx->stats_interval);

	lea = evlearner_init(argv[1], deliver, ctx, ctx->base);
	if (lea == NULL) {
		printf("Could not start the learner!\n");
		exit(1);
	}
	
	sig = evsignal_new(ctx->base, SIGINT, handle_sigint, ctx->base);
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
