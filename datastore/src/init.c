#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "datastore.h"

extern char DBPath[];

int init_rocksdb(struct rocksdb_params *lp) {
    char *err = NULL;
    int ret;
    lp->options = rocksdb_options_create();
    // Optimize RocksDB. This is the easiest way to
    // get RocksDB to perform well
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);  // get # of online cores
    rocksdb_options_increase_parallelism(lp->options, (int)(cpus));
    // rocksdb_options_optimize_level_style_compaction(lp->options, 0);
    rocksdb_options_optimize_level_style_compaction(lp->options, rocksdb_configurations.mem_budget);
    // create the DB if it's not already present
    rocksdb_options_set_create_if_missing(lp->options, 1);
    // rocksdb_options_set_write_buffer_size(lp->options, DEFAULT_WRITE_BUFFER_SIZE);
    lp->writeoptions = rocksdb_writeoptions_create();
    rocksdb_writeoptions_disable_WAL(lp->writeoptions, rocksdb_configurations.disable_wal);
    rocksdb_writeoptions_set_sync(lp->writeoptions, rocksdb_configurations.write_sync);
    if (rocksdb_configurations.enable_statistics) {
        rocksdb_options_enable_statistics(lp->options);
    }

    gethostname(lp->hostname, 32);

    uint32_t i;
    for (i = 0; i < lp->num_workers; i++) {
        if (rocksdb_configurations.rm_existed_db) {
            char cmd[256];
            snprintf(cmd, 256, "exec rm -rf %s", rocksdb_configurations.db_paths[i]);
            printf("Remove DB if existed: %s\n", cmd);
            ret = system(cmd);
            if (ret < 0) {
                printf("Cannot remove db dir\n");
                return -1;
            }
        }

        if (rocksdb_configurations.sync_db) {
            char cmd[256];
            char* hostname = strtok(rocksdb_configurations.replica_hostname, "\n");
            snprintf(cmd, 256, "exec rsync -azr --delete %s:%s %s",
            hostname,
            rocksdb_configurations.db_paths[i], rocksdb_configurations.db_paths[i]);
            printf("%s\n", cmd);
            ret = system(cmd);
            if (ret < 0) {
                printf("Cannot sync db dir\n");
                return -1;
            }
        }

        lp->worker[i].db_path = rocksdb_configurations.db_paths[i];

        lp->worker[i].db = rocksdb_open(lp->options, rocksdb_configurations.db_paths[i], &err);
        if (err != NULL) {
            fprintf(stderr, "Cannot open DB: %s\n", err);
            return -1;
        }

        lp->worker[i].cp = rocksdb_checkpoint_object_create(lp->worker[i].db, &err);
        if (err != NULL) {
            fprintf(stderr, "Cannot create checkpoint object: %s\n", err);
            return -1;
        }
        // Clean checkpoint directory
        char cmd[256];
        snprintf(cmd, 256, "exec rm -rf %s/checkpoints", rocksdb_configurations.db_paths[i]);
        ret = system(cmd);
        if (ret < 0) {
            printf("Cannot remove Checkpoint dir\n");
            return -1;
        }
        snprintf(cmd, 256, "mkdir -p %s/checkpoints", rocksdb_configurations.db_paths[i]);
        ret = system(cmd);
        if (ret < 0) {
            printf("Cannot remove Checkpoint dir\n");
            return -1;
        }

    }
    lp->flops = rocksdb_flushoptions_create();
    lp->readoptions = rocksdb_readoptions_create();

    return 0;
}
