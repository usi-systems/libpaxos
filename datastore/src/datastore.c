#include <stdio.h>
#include <stdlib.h>
#include "datastore.h"

void handle_put(struct rocksdb_t *db, struct rocksdb_writeoptions_t *writeoptions,
                const char *key, uint32_t keylen,
                const char *value, uint32_t vallen) {
    char *err = NULL;
    rocksdb_put(db, writeoptions, key, keylen, value, vallen, &err);
    if (err != NULL) {
        fprintf(stderr, "Write Error: %s\n", err);
    }
}

char* handle_get(struct rocksdb_t *db, struct rocksdb_readoptions_t *readoptions,
                const char *key, uint32_t keylen, size_t *vallen) {
    char *err = NULL;
    char *retval = rocksdb_get(db, readoptions, key, keylen, vallen, &err);
    if (err != NULL) {
        fprintf(stderr, "Read Error: %s\n", err);
        return NULL;
    }
    return retval;
}


void handle_checkpoint(struct rocksdb_checkpoint_t *cp, const char *cp_path) {
    char *err = NULL;
    rocksdb_checkpoint_create(cp, cp_path, rocksdb_configurations.flush_size, &err);
    if (err != NULL) {
        fprintf(stderr, "Checkpoint Error: %s\n", err);
    }
}


void display_rocksdb_statistics(struct rocksdb_params *lp)
{
    if (rocksdb_configurations.enable_statistics) {
        char *stats = rocksdb_options_statistics_get_string(lp->options);
        printf("\n\n%s\n", stats);
        free(stats);
    }
}
