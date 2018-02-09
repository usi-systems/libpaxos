/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>

#include "main.h"
#include "app_hdr.h"

const char DBPath[] = "/tmp/";

struct rocksdb_params rocks;


static void
init_rocksdb(void)
{
	char *err = NULL;
	uint64_t mem_budget = 1048576;
	rocks.options = rocksdb_options_create();
	long cpus = sysconf(_SC_NPROCESSORS_ONLN);
	rocksdb_options_increase_parallelism(rocks.options, (int)(cpus));
	rocksdb_options_optimize_level_style_compaction(rocks.options, mem_budget);
	// create the DB if it's not already present
    rocksdb_options_set_create_if_missing(rocks.options, 1);
    // Write asynchronously
    rocks.writeoptions = rocksdb_writeoptions_create();
	// Disable WAL (Flush to disk manually)
	rocksdb_writeoptions_disable_WAL(rocks.writeoptions, 1);
    rocksdb_writeoptions_set_sync(rocks.writeoptions, 0);
	rocks.readoptions = rocksdb_readoptions_create();

	char db_name[DB_NAME_LENGTH];
	snprintf(db_name, DB_NAME_LENGTH, "%s/p4xos-rocksdb", DBPath);
	rocks.db = rocksdb_open(rocks.options, db_name, &err);
	if (err != NULL) {
	  rte_panic("Cannot open DB: %s\n", err);
	}
	rocks.wrbatch = rocksdb_writebatch_create();
	rocks.cp = rocksdb_checkpoint_object_create(rocks.db, &err);
	if (err != NULL) {
	  rte_panic("Cannot create checkpoint object: %s\n", err);
	}
	rocks.flops = rocksdb_flushoptions_create();
}

static
void deliver(unsigned int __rte_unused inst, __rte_unused char* val,
			__rte_unused size_t size, __rte_unused void* arg) {
	char *err = NULL;
	struct rocksdb_params *rocks = (struct rocksdb_params *)arg;
	struct app_hdr *ap = (struct app_hdr *)val;
	// printf("inst %d, type: %d, key %s, value %s\n", inst, ap->msg_type, ap->key, ap->value);
	if (ap->msg_type == WRITE_OP) {
		uint32_t key_len = rte_be_to_cpu_32(ap->key_len);
		uint32_t value_len = rte_be_to_cpu_32(ap->value_len);
		// printf("Key %s, Value %s\n", ap->key, ap->value);
		// // Single PUT
		rocksdb_put(rocks->db, rocks->writeoptions, (const char*)ap->key, key_len,
		(const char*)ap->value, value_len, &err);
		if (err != NULL) {
			printf("Write Error: %s\n", err);
		}
		// // Write Batch
		// rocksdb_writebatch_put(rocks->wrbatch, (const char*)ap->key, key_len,
		// (const char*)ap->value, value_len);
		// if (rocksdb_writebatch_count(rocks->wrbatch) == ROCKSDB_WRITEBATCH_SIZE) {
		// 	rocksdb_write(rocks->db, rocks->writeoptions, rocks->wrbatch, &err);
		// 	if (err != NULL) {
		// 		printf("WriteBatch Error: %s\n", err);
		// 	}
		// 	rocksdb_flush(rocks->db, rocks->flops, &err);
		// 	if (err != NULL) {
		// 		printf("Flush to disk Error: %s\n", err);
		// 	}
		// 	rocksdb_writebatch_clear(rocks->wrbatch);
		// }
		rocks->write_count++;
	}
	else if (ap->msg_type == READ_OP) {
		size_t len;
		uint32_t key_len = rte_be_to_cpu_32(ap->key_len);
		// printf("Key %s\n", ap->key);
	    char *returned_value =
	        rocksdb_get(rocks->db, rocks->readoptions, (const char*)ap->key, key_len, &len, &err);
		// printf("return value %s\n", returned_value);
		rte_memcpy(ap->value, returned_value, len);
	    free(returned_value);
		rocks->read_count++;
	}

	rocks->delivered_count++;
}

int
main(int argc, char **argv)
{
	uint32_t lcore;
	int ret;

	/* Init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		return -1;
	argc -= ret;
	argv += ret;

	/* Parse application arguments (after the EAL ones) */
	ret = app_parse_args(argc, argv);
	if (ret < 0) {
		app_print_usage();
		return -1;
	}

	/* Init */
	app_init();
	app_print_params();
	init_rocksdb();
	app_set_deliver_callback(deliver);
	app_set_deliver_arg(&rocks);
	app_set_worker_callback(learner_handler);
	/* Launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(app_lcore_main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		if (rte_eal_wait_lcore(lcore) < 0) {
			return -1;
		}
	}

	return 0;
}
