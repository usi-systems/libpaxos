#ifndef _ROCKSDB_CONTEXT_H_
#define _ROCKSDB_CONTEXT_H_

#include <rocksdb/c.h>

struct rocksdb_ctx {
    rocksdb_t *db;
    rocksdb_options_t *options;
    rocksdb_readoptions_t *roptions;
    rocksdb_writeoptions_t *woptions;
};

struct rocksdb_ctx* new_rocksdb_context();
void open_db(struct rocksdb_ctx *ctx, const char* db_name);
void destroy_db(struct rocksdb_ctx *ctx, const char* db_name);
int add_entry(struct rocksdb_ctx *ctx, int sync, char *key, int ksize,
                char* val, int vsize);
int get_value(struct rocksdb_ctx *ctx, char *key, size_t ksize, char** val,
                size_t* vsize);
int delete_entry(struct rocksdb_ctx *ctx, char *key, int ksize);
void free_db_context(struct rocksdb_ctx *ctx);

#endif
