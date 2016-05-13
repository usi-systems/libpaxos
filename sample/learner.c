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
 /* levelDB */
#include <leveldb/c.h>

#include "application.h"

struct application_ctx
{
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
};

struct application_ctx* new_application() {
    struct application_ctx *ctx = malloc(sizeof (struct application_ctx));
    return ctx;
}


void open_db(struct application_ctx *ctx, char* db_name) {
    char *err = NULL;
    leveldb_options_set_create_if_missing(ctx->options, true);
    ctx->db = leveldb_open(ctx->options, db_name, &err);
    if (err != NULL) {
        fprintf(stderr, "Open fail.\n");
        leveldb_free(err);
        exit (EXIT_FAILURE);
    }
}

void destroy_db(struct application_ctx *ctx, char* db_name) {
    char *err = NULL;
    leveldb_destroy_db(ctx->options, db_name, &err);
    if (err != NULL) {
        fprintf(stderr, "Open fail.\n");
        leveldb_free(err);
        exit (EXIT_FAILURE);
    }
}

int add_entry(struct application_ctx *ctx, int sync, char *key, int ksize, char* val, int vsize) {
    char *err = NULL;
    leveldb_writeoptions_set_sync(ctx->woptions, sync);
    leveldb_put(ctx->db, ctx->woptions, key, ksize, val, vsize, &err);
    if (err != NULL) {
        fprintf(stderr, "Write fail.\n");
        leveldb_free(err); err = NULL;
        return(1);
    }
    return 0;
}

int get_value(struct application_ctx *ctx, char *key, size_t ksize, char** val, size_t* vsize) {
    char *err = NULL;
    *val = leveldb_get(ctx->db, ctx->roptions, key, ksize, vsize, &err);
    if (err != NULL) {
        fprintf(stderr, "Read fail.\n");
        leveldb_free(err); err = NULL;
        return(1);
    }
    return 0;
}

int delete_entry(struct application_ctx *ctx, char *key, int ksize) {
    char *err = NULL;
    leveldb_delete(ctx->db, ctx->woptions, key, ksize, &err);
    if (err != NULL) {
        fprintf(stderr, "Delete fail.\n");
        leveldb_free(err); err = NULL;
        return(1);
    }
    return 0;
}

void init_application(struct application_ctx *ctx) {
    ctx->options = leveldb_options_create();
    ctx->woptions = leveldb_writeoptions_create();
    ctx->roptions = leveldb_readoptions_create();
    open_db(ctx, "/tmp/libpaxos");
}


void free_application(struct application_ctx *ctx) {
    leveldb_close(ctx->db);
    leveldb_writeoptions_destroy(ctx->woptions);
    leveldb_readoptions_destroy(ctx->roptions);
    leveldb_options_destroy(ctx->options);
}

static void
handle_sigint(int sig, short ev, void* arg)
{
	struct event_base* base = arg;
	printf("Caught signal %d\n", sig);
	event_base_loopexit(base, NULL);
}

static void
deliver(unsigned iid, char* value, size_t size, void* arg)
{
	struct application_ctx *ctx = arg;
	struct client_value* val = (struct client_value*)value;
	if (val->application_type == LEVELDB) {
		struct leveldb_request *req = (struct leveldb_request *) &val->content;
		switch(req->op) {
			case PUT: {
				 printf("PUT: key %s: %zu, value %s: %zu\n", req->key, req->ksize, req->value, req->vsize);
				 add_entry(ctx, false, req->key, req->ksize, req->value, req->vsize);
				 break;
		}
			case GET: {
				 char *value;
				 size_t vsize = 0;
				 get_value(ctx, req->key, req->ksize, &value, &vsize);
				 printf("GET: key %s: %zu, RETURN: %s %zu\n", req->key, req->ksize, value, vsize);
				 if (value)
				     free(value);
				 break;
		}
    }

	} else if (val->application_type == SIMPLY_ECHO) {
		struct echo_request *req = (struct echo_request *)&val->content;
		printf("%ld.%06ld [%.32s] %ld bytes\n",
			val->depart_ts.tv_sec,
			val->depart_ts.tv_usec,
			req->value,
			(long)val->size);
	}
}

static void
start_learner(const char* config)
{
	struct event* sig;
	struct evlearner* lea;
	struct event_base* base;

	base = event_base_new();

	struct application_ctx *ctx = new_application();
	init_application(ctx);

	lea = evlearner_init(config, deliver, ctx, base);
	if (lea == NULL) {
		printf("Could not start the learner!\n");
		exit(1);
	}
	
	sig = evsignal_new(base, SIGINT, handle_sigint, base);
	evsignal_add(sig, NULL);

	signal(SIGPIPE, SIG_IGN);
	event_base_dispatch(base);

	event_free(sig);
	evlearner_free(lea);
	event_base_free(base);
	free_application(ctx);
}

int
main(int argc, char const *argv[])
{
	const char* config = "../paxos.conf";

	if (argc != 1 && argc != 2) {
		printf("Usage: %s [path/to/paxos.conf]\n", argv[0]);
		exit(1);
	}

	if (argc == 2)
		config = argv[1];

	start_learner(config);
	
	return 0;
}
