//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>   // isprint
#include <unistd.h>  // sysconf() - get CPU count
#include "rocksdb_context.h"

const char DBPath[] = "/tmp/toysdb";
const char DBCheckpointPath[] = "/tmp/checkpoints";

enum boolean {
    false,
    true
};

struct rocksdb_ctx* new_rocksdb_context() {
  struct rocksdb_ctx *ctx = malloc(sizeof (struct rocksdb_ctx));
  ctx->options = rocksdb_options_create();
  ctx->woptions = rocksdb_writeoptions_create();
  ctx->roptions = rocksdb_readoptions_create();
  open_db(ctx, DBPath);
  rocksdb_writeoptions_set_sync(ctx->woptions, false);
  return ctx;
}

void open_db(struct rocksdb_ctx *ctx, const char* db_name) {
  char *err = NULL;
  uint64_t mem_budget = 1048576;
  rocksdb_options_optimize_level_style_compaction(ctx->options, mem_budget);
  rocksdb_options_set_create_if_missing(ctx->options, true);
  ctx->db = rocksdb_open(ctx->options, db_name, &err);
  if (err != NULL) {
      fprintf(stderr, "Open fail.\n");
      exit (EXIT_FAILURE);
  }
}

void destroy_db(struct rocksdb_ctx *ctx, const char* db_name) {
  char *err = NULL;
  rocksdb_destroy_db(ctx->options, db_name, &err);
  if (err != NULL) {
      fprintf(stderr, "Destroy fail.\n");
      exit (EXIT_FAILURE);
  }
}

int add_entry(struct rocksdb_ctx *ctx, int sync, char *key, int ksize,
                char* val, int vsize) {
  char *err = NULL;
  rocksdb_put(ctx->db, ctx->woptions, key, ksize, val, vsize, &err);
  if (err != NULL) {
    fprintf(stderr, "Write Error: %s\n", err);
    exit (EXIT_FAILURE);
  }
  return 0;
}

int get_value(struct rocksdb_ctx *ctx, char *key, size_t ksize, char** val,
                size_t* vsize) {
  char *err = NULL;
  *val = rocksdb_get(ctx->db, ctx->roptions, key, ksize, vsize, &err);
  if (err) {
    fprintf(stderr, "Read Error: %s\n", err);
    return 1;
  }
  return 0;
}

int delete_entry(struct rocksdb_ctx *ctx, char *key, int ksize) {
  char *err = NULL;
  rocksdb_delete(ctx->db, ctx->woptions, key, ksize, &err);
  if (err != NULL) {
      fprintf(stderr, "Delete fail.\n");
      return 1;
  }
  return 0;
}

void free_db_context(struct rocksdb_ctx *ctx) {
  rocksdb_close(ctx->db);
  rocksdb_writeoptions_destroy(ctx->woptions);
  rocksdb_readoptions_destroy(ctx->roptions);
  rocksdb_options_destroy(ctx->options);
  free(ctx);
}
