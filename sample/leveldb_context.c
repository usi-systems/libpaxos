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
#include <string.h>
#include "leveldb_context.h"

struct leveldb_ctx* new_leveldb_context() {
    struct leveldb_ctx *ctx = malloc(sizeof (struct leveldb_ctx));
    ctx->options = leveldb_options_create();
    ctx->woptions = leveldb_writeoptions_create();
    ctx->roptions = leveldb_readoptions_create();
    open_db(ctx, "/tmp/libpaxos");
    return ctx;
}


void open_db(struct leveldb_ctx *ctx, char* db_name) {
    char *err = NULL;
    leveldb_options_set_create_if_missing(ctx->options, true);
    ctx->db = leveldb_open(ctx->options, db_name, &err);
    if (err != NULL) {
        fprintf(stderr, "Open fail.\n");
        leveldb_free(err);
        exit (EXIT_FAILURE);
    }
}

void destroy_db(struct leveldb_ctx *ctx, char* db_name) {
    char *err = NULL;
    leveldb_destroy_db(ctx->options, db_name, &err);
    if (err != NULL) {
        fprintf(stderr, "Open fail.\n");
        leveldb_free(err);
        exit (EXIT_FAILURE);
    }
}

int add_entry(struct leveldb_ctx *ctx, int sync, char *key, int ksize, char* val, int vsize) {
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

int get_value(struct leveldb_ctx *ctx, char *key, size_t ksize, char** val, size_t* vsize) {
    char *err = NULL;
    *val = leveldb_get(ctx->db, ctx->roptions, key, ksize, vsize, &err);
    if (err != NULL) {
        fprintf(stderr, "Read fail.\n");
        leveldb_free(err); err = NULL;
        return(1);
    }
    return 0;
}

int delete_entry(struct leveldb_ctx *ctx, char *key, int ksize) {
    char *err = NULL;
    leveldb_delete(ctx->db, ctx->woptions, key, ksize, &err);
    if (err != NULL) {
        fprintf(stderr, "Delete fail.\n");
        leveldb_free(err); err = NULL;
        return(1);
    }
    return 0;
}

void free_leveldb_context(struct leveldb_ctx *ctx) {
    leveldb_close(ctx->db);
    leveldb_writeoptions_destroy(ctx->woptions);
    leveldb_readoptions_destroy(ctx->roptions);
    leveldb_options_destroy(ctx->options);
    free(ctx);
}


