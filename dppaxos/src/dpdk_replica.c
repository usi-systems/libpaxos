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

static uint8_t DEFAULT_VALUE[] = "ABCDEFGH1234567";
static uint64_t log_size_for_flush = 1048576;
static uint8_t msg_type = READ_OP;
static uint8_t key[KEY_LEN];

static void
set_app_hdr(struct app_hdr *ap, uint32_t inst, uint8_t msg_type, uint32_t key_len,
	uint8_t* key, uint32_t value_len, uint8_t* value) {
	ap->msg_type = msg_type;
	ap->key_len = rte_cpu_to_be_32(key_len);
	ap->value_len = rte_cpu_to_be_32(value_len);
	rte_memcpy(ap->key, key, key_len);
	if (ap->msg_type == WRITE_OP) {
		rte_memcpy(ap->value, value, value_len);
	}
}


static inline void submit_new_commands(void) {
	struct app_hdr ap;
	uint32_t i;
	uint32_t pos_lb = app.pos_lb - 42 - 16 - 5;
	for (i = 1; i < app.p4xos_conf.osd; i++) {
		key[pos_lb] = i;
		set_app_hdr(&ap, i, msg_type, KEY_LEN, key, sizeof(DEFAULT_VALUE), DEFAULT_VALUE);
		// submit_all_ports((char*)&ap, sizeof(struct app_hdr));
		submit((char*)&ap, sizeof(struct app_hdr));
	}
}


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
	if (app.p4xos_conf.multi_dbs) {
		rocks.num_workers = app_get_lcores_worker();
	}
	else {
		rocks.num_workers = 1;
	}
	char db_name[DB_NAME_LENGTH];
	uint32_t i;
	for (i=0; i < rocks.num_workers; i++) {
		snprintf(db_name, DB_NAME_LENGTH, "%s/p4xos-rocksdb-worker-%u", DBPath, i);
		rocks.db[i] = rocksdb_open(rocks.options, db_name, &err);
		if (err != NULL) {
			rte_panic("Cannot open DB: %s\n", err);
		}
		rocks.wrbatch[i] = rocksdb_writebatch_create();
		rocks.cp[i] = rocksdb_checkpoint_object_create(rocks.db[i], &err);
		if (err != NULL) {
			rte_panic("Cannot create checkpoint object: %s\n", err);
		}
	}
	rocks.flops = rocksdb_flushoptions_create();
}

static
void deliver(unsigned int worker_id, unsigned int __rte_unused inst, __rte_unused char* val,
			__rte_unused size_t size, __rte_unused void* arg) {
	char *err = NULL;
	if (rocks.num_workers == 1) {
		worker_id = 0;
	}
	struct rocksdb_params *rocks = (struct rocksdb_params *)arg;
	struct app_hdr *ap = (struct app_hdr *)val;
	// printf("inst %d, type: %d, key %s, value %s\n", inst, ap->msg_type, ap->key, ap->value);
	if (ap->msg_type == WRITE_OP) {
		uint32_t key_len = rte_be_to_cpu_32(ap->key_len);
		uint32_t value_len = rte_be_to_cpu_32(ap->value_len);
		// printf("Key %s, Value %s\n", ap->key, ap->value);
		// // Single PUT
		rocksdb_put(rocks->db[worker_id], rocks->writeoptions, (const char*)ap->key, key_len,
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
		rocks->write_count[worker_id]++;
	}
	else if (ap->msg_type == READ_OP) {
		size_t len;
		uint32_t key_len = rte_be_to_cpu_32(ap->key_len);
		// printf("Key %s\n", ap->key);
	    char *returned_value =
	        rocksdb_get(rocks->db[worker_id], rocks->readoptions, (const char*)ap->key, key_len, &len, &err);
		printf("Key %s: return value %s\n", ap->key, returned_value);
		rte_memcpy(ap->value, returned_value, len);
	    free(returned_value);
		rocks->read_count[worker_id]++;
	}

	rocks->delivered_count[worker_id]++;

	if (app.p4xos_conf.checkpoint_interval > 0 && (inst % app.p4xos_conf.checkpoint_interval == 0)) {
		char cp_path[DB_NAME_LENGTH];
		snprintf(cp_path, DB_NAME_LENGTH, "%s/checkpoints/worker-%u-inst-%u", DBPath, worker_id, inst);
		rocksdb_checkpoint_create(rocks->cp[worker_id], cp_path, log_size_for_flush, &err);
		if (err != NULL) {
			printf("Checkpoint Error: %s\n", err);
		}
	}
}

static void
stat_cb(__rte_unused struct rte_timer *timer, __rte_unused void *arg)
{
	uint32_t lcore = 0;
	uint32_t nb_delivery = 0, nb_latency = 0;
	uint64_t latency = 0;
	uint64_t total_pkts = 0, total_bytes = 0;
	uint32_t i;

	for (lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;

		if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
			continue;
		}

		total_pkts += lp->total_pkts;
		total_bytes += lp->total_bytes;
		lp->total_pkts = 0;
		lp->total_bytes = 0;
		nb_latency += lp->nb_latency;
		nb_delivery += lp->nb_delivery;
		latency += lp->latency;
		lp->nb_latency = 0;
		lp->nb_delivery = 0;
		lp->latency = 0;
	}

	struct rocksdb_params *rocks = (struct rocksdb_params *)arg;
	uint32_t delivered_count = 0;
	for (i = 0; i < rocks->num_workers; i++) {
		delivered_count += rocks->delivered_count[i];
		if (rocks->delivered_count[i] > 0) {
			printf("Worker %u: delivered %u\t", i, rocks->delivered_count[i]);
		}
		rocks->delivered_count[i] = 0;
	}
	if (delivered_count > 0) {
		double avg_cycle_latency = (double) latency / (double) nb_latency;
		double avg_ns_latency = avg_cycle_latency * NS_PER_S / app.hz;
		printf("\nThroughput = %"PRIu64" pkts, %2.1f Gbits; "
				"Learner Throughput %u\n"
				"Avg latency = %.2f cycles ~ %.1f ns\n",
				total_pkts, bytes_to_gbits(total_bytes),
				delivered_count,
				avg_cycle_latency, avg_ns_latency);
	}
	 else {
		 printf("Resubmmit new commands\n");
		 submit_new_commands();
	}
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
	app_init_learner();
	app_init_acceptor();
	app_print_params();
	init_rocksdb();
	app_set_deliver_callback(deliver, &rocks);
	app_set_worker_callback(replica_handler);
	app_set_stat_callback(stat_cb, &rocks);

	struct app_hdr ap;
	uint32_t i;
	uint32_t n_workers = app_get_lcores_worker();
	uint32_t pos_lb = app.pos_lb - 42 - 16 - 5;
	printf("pos lb %d\n", pos_lb);
	for (i = 0; i < n_workers; i++) {
		key[pos_lb] = i;
		set_app_hdr(&ap, i, READ_OP, KEY_LEN, key, 0, NULL);
		reset_leader_instance((char*)&ap, sizeof(struct app_hdr));
	}


	uint8_t value[VALUE_LEN];
	for (i = 0; i < app.p4xos_conf.osd; i++) {
		// snprintf((char *)key, KEY_LEN, "%06d", i);
		key[pos_lb] = i%n_workers;
		snprintf((char *)value, VALUE_LEN, "VALUE%07d", i);
 		msg_type = WRITE_OP;
		set_app_hdr(&ap, i, msg_type, KEY_LEN, key, VALUE_LEN, value);
		if (app.p4xos_conf.all_ports) {
			submit_all_ports((char*)&ap, sizeof(struct app_hdr));
		} else {
			submit((char*)&ap, sizeof(struct app_hdr));
		}
	}
	app_set_default_value((char*)&ap, sizeof(struct app_hdr));

	/* Launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(app_lcore_main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		if (rte_eal_wait_lcore(lcore) < 0) {
			return -1;
		}
	}

	return 0;
}
