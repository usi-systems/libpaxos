#include <stdio.h>
#include <stdlib.h>
#include "datastore.h"

void handle_put(struct rocksdb_params *lp, const char *key, uint32_t keylen,
                const char *value, uint32_t vallen) {
    char *err = NULL;
    rocksdb_put(lp->db[lp->lcore_id], lp->writeoptions, key, keylen,
                value, vallen, &err);
    if (err != NULL) {
        fprintf(stderr, "Write Error: %s\n", err);
    }
}

void handle_get(struct rocksdb_params *lp, const char *key, uint32_t keylen) {
    char *err = NULL;
    size_t vallen;
    char *retval = rocksdb_get(lp->db[lp->lcore_id], lp->readoptions, key,
                               keylen, &vallen, &err);
    if (err != NULL) {
        fprintf(stderr, "Read Error: %s\n", err);
        return;
    }

    if (retval != NULL) {
        printf("Returned value %s\n", retval);
        free(retval);
    }
}


void handle_checkpoint(struct rocksdb_params *lp, const char *cp_path) {
    char *err = NULL;
    rocksdb_checkpoint_create(lp->cp[lp->lcore_id], cp_path, rocksdb_configurations.flush_size, &err);
    if (err != NULL) {
        fprintf(stderr, "Checkpoint Error: %s\n", err);
    }
}

void display_rocksdb_statistics(struct rocksdb_params *lp)
{
    if (rocksdb_configurations.enable_statistics) {
        char *stats = rocksdb_options_statistics_get_string(lp->options);
        printf("\n\nLcore %u\n%s\n", lp->lcore_id, stats);
        free(stats);
    }
}
