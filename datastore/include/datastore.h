#ifndef _DATASTORE_H_
#define _DATASTORE_H_

#include <inttypes.h>
#include <stdint.h>

#include "rocksdb/c.h"

#ifndef FILENAME_LENGTH
#define FILENAME_LENGTH 128
#endif

#define MAX_WORKER_CORE 4

#define HOSTNAME_LENGTH 128

#define APP_ARG_NUMERICAL_SIZE_CHARS 32
#define CHUNK_SIZE 4096

#define DEFAULT_WRITE_SYNC 0
#define DEFAULT_DISABLE_WAL 0
#define DEFAULT_DISABLE_FSYNC 0
#define DEFAULT_DISABLE_CHECKPOINT 0
#define DEFAULT_DISABLE_STATISTICS 0
#define DEFAULT_RM_EXISTED_DB 0
#define DEFAULT_MEM_BUDGET 512 * 1024 * 1024
#define DEFAULT_LOG_SIZE_FLUSH 4 * 1024 * 1024
#define DEFAULT_WRITE_BUFFER_SIZE 4 * 1024 * 1024
#define DEFAULT_NB_KEYS 1000000
#define READ_REQ  1
#define WRITE_REQ 2
#define MAX_NB_PARTITION 32

struct rocksdb_configurations {
  uint8_t write_sync;
  uint8_t use_fsync;
  uint8_t disable_wal;
  uint8_t enable_checkpoint;
  uint8_t enable_statistics;
  uint8_t rm_existed_db;
  uint32_t mem_budget;
  uint32_t flush_size;
  uint32_t partition_count;
  uint32_t nb_keys;
  char* db_paths[MAX_NB_PARTITION];
};

struct rocksdb_lcore_params {
    char *db_path;
    rocksdb_t *db;
    rocksdb_checkpoint_t *cp;
    uint32_t delivered_count;
    uint32_t write_count;
    uint32_t read_count;
};

struct rocksdb_params {
    /* Rocksdb */
    struct rocksdb_lcore_params worker[MAX_WORKER_CORE];
    rocksdb_options_t *options;
    rocksdb_writeoptions_t *writeoptions;
    rocksdb_readoptions_t *readoptions;
	rocksdb_flushoptions_t* flops;
    uint32_t num_workers;
    uint32_t total_delivered_count;
    char hostname[32];
};

#define KEYLEN 4
#define VALLEN 2

struct request {
    uint32_t key;
    uint16_t value;
    uint8_t type;
    uint8_t terminator;
}  __attribute__((__packed__));

extern struct rocksdb_configurations rocksdb_configurations;

void rocksdb_print_usage(void);
int parse_rocksdb_configuration(int argc, char **argv);
void populate_configuration(char *config_file, struct rocksdb_configurations *conf);
void free_rocksdb_configurations(struct rocksdb_configurations *conf);
void print_parameters(void);
int init_rocksdb(struct rocksdb_params *lp);
void handle_put(struct rocksdb_t *db, struct rocksdb_writeoptions_t *writeoptions,
                const char *key, uint32_t keylen,
                const char *value, uint32_t vallen);
char* handle_get(struct rocksdb_t *db, struct rocksdb_readoptions_t *readoptions,
                const char *key, uint32_t keylen, size_t *vallen);
void handle_checkpoint(struct rocksdb_checkpoint_t *cp, const char *cp_path);
void display_rocksdb_statistics(struct rocksdb_params *lp);
#endif // _DATASTORE_H_
