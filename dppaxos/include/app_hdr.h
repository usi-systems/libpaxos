#ifndef _APP_HDR_H_
#define _APP_HDR_H_

#define KEY_LEN 3
#define VALUE_LEN 3
#define READ_OP 1
#define WRITE_OP 2

/* Paxos logic */
#ifndef DB_NAME_LENGTH
#define DB_NAME_LENGTH 128
#endif
#if (DB_NAME_LENGTH > 128)
#error "DB_NAME_LENGTH is too big"
#endif

#ifndef ROCKSDB_WRITEBATCH_SIZE
#define ROCKSDB_WRITEBATCH_SIZE 144
#endif
#if (ROCKSDB_WRITEBATCH_SIZE > 144)
#error "ROCKSDB_WRITEBATCH_SIZE is too big"
#endif

#ifndef MAX_WORKER_CORE
#define MAX_WORKER_CORE 4
#endif
#if (MAX_WORKER_CORE > 8)
#error "ROCKSDB_WRITEBATCH_SIZE is too big"
#endif

#include "rocksdb/c.h"

#ifdef __cplusplus
extern "C" {
#endif

struct app_hdr {
    uint8_t msg_type;
    uint32_t key_len;
	uint8_t key[KEY_LEN];
    uint32_t value_len;
	uint8_t value[VALUE_LEN];
} __attribute__((__packed__));

struct rocksdb_params {
    /* Rocksdb */
	rocksdb_t *db[MAX_WORKER_CORE];
    rocksdb_options_t *options;
    rocksdb_writeoptions_t *writeoptions;
    rocksdb_readoptions_t *readoptions;
	rocksdb_writebatch_t* wrbatch[MAX_WORKER_CORE];
	rocksdb_checkpoint_t *cp[MAX_WORKER_CORE];
	rocksdb_flushoptions_t* flops;
    uint32_t delivered_count[MAX_WORKER_CORE];
    uint32_t write_count[MAX_WORKER_CORE];
    uint32_t read_count[MAX_WORKER_CORE];
    uint64_t last_cycle[MAX_WORKER_CORE];
    uint32_t num_workers;
    uint32_t total_delivered_count;
};

#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif
