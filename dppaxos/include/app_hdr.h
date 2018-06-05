#ifndef _APP_HDR_H_
#define _APP_HDR_H_

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
    uint8_t key;
    uint16_t value;
} __attribute__((__packed__));


#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif
