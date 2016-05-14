#include <leveldb/c.h>
#include "application.h"

struct leveldb_ctx {
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
};
struct leveldb_ctx* new_leveldb_context();
void open_db(struct leveldb_ctx *ctx, char* db_name);
void destroy_db(struct leveldb_ctx *ctx, char* db_name);
int add_entry(struct leveldb_ctx *ctx, int sync, char *key, int ksize,
                char* val, int vsize);
int get_value(struct leveldb_ctx *ctx, char *key, size_t ksize, char** val,
                size_t* vsize);
int delete_entry(struct leveldb_ctx *ctx, char *key, int ksize);
void free_leveldb_context(struct leveldb_ctx *ctx);