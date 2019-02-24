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


void handle_backup(struct rocksdb_t *db, rocksdb_backup_engine_t *be) {
    char *err = NULL;
    // create new backup in a directory specified by DBBackupPath
    rocksdb_backup_engine_create_new_backup(be, db, &err);
    if (err != NULL) {
        fprintf(stderr, "Backup Error: %s\n", err);
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

void lcore_cleanup(struct rocksdb_lcore_params *lp)
{
    if (rocksdb_configurations.enable_checkpoint) {
        rocksdb_checkpoint_object_destroy(lp->cp);
    }
    rocksdb_backup_engine_close(lp->be);
    rocksdb_close(lp->db);
}

void cleanup(struct rocksdb_params *lp)
{
    // cleanup
    printf("Successfully Clean up\n");

    rocksdb_writeoptions_destroy(lp->writeoptions);
    rocksdb_readoptions_destroy(lp->readoptions);
    rocksdb_options_destroy(lp->options);
    unsigned i = 0;
    for (; i < lp->num_workers; i++) {
        lcore_cleanup(&lp->worker[i]);
    }
}
