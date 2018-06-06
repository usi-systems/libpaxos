/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <unistd.h>

#include "main.h"
#include "datastore.h"

struct rocksdb_params rocks;
struct rocksdb_configurations rocksdb_configurations;


static void baseline_deliver(unsigned int worker_id,
                             unsigned int __rte_unused inst,
                             __rte_unused char *val, __rte_unused size_t size,
                             __rte_unused void *arg) {
  if (rocks.num_workers == 1) {
    worker_id = 0;
  }
  struct rocksdb_params *rocks = (struct rocksdb_params *)arg;
  rocks->delivered_count[worker_id]++;
  if (app.p4xos_conf.checkpoint_interval > 0 &&
      (inst % app.p4xos_conf.checkpoint_interval == 0)) {
    send_checkpoint_message(worker_id, inst);
  }
}

static void deliver(unsigned int worker_id, unsigned int __rte_unused inst,
                    __rte_unused char *val, __rte_unused size_t size,
                    __rte_unused void *arg) {
    if (rocks.num_workers == 1) {
        worker_id = 0;
    }
    struct rocksdb_params *rocks = (struct rocksdb_params *)arg;
    struct request *ap = (struct request *)val;
    // printf("inst %d, type: %d, key %s, value %s\n", inst, ap->msg_type,
    // ap->key, ap->value);
    if (ap->type == WRITE_REQ) {
        uint32_t key_len = 1;   // rte_be_to_cpu_32(ap->key_len);
        uint32_t value_len = 2; // rte_be_to_cpu_32(ap->value_len);
        // printf("Key %s, Value %s\n", ap->key, ap->value);
        // // Single PUT
        handle_put(rocks, (const char *)&ap->key,
                    key_len, (const char *)&ap->value, value_len);
        rocks->write_count[worker_id]++;

    } else if (ap->type == READ_REQ) {
        size_t len;
        uint32_t key_len = 1; // rte_be_to_cpu_32(ap->key_len);
        // printf("Key %s\n", ap->key);
        char *returned_value =
        handle_get(rocks, (const char *)&ap->key, key_len, &len);
        if (returned_value != NULL) {
            // printf("Key %s: return value %s\n", ap->key, returned_value);
            rte_memcpy((char *)&ap->value, returned_value, len);
            free(returned_value);
        }
        rocks->read_count[worker_id]++;
    }

    rocks->delivered_count[worker_id]++;

    if (inst > 0 && app.p4xos_conf.checkpoint_interval > 0 &&
      (inst % app.p4xos_conf.checkpoint_interval == 0)) {
    // char cp_path[DB_NAME_LENGTH];
    // snprintf(cp_path, DB_NAME_LENGTH, "%s/checkpoints/%s-core-%u-inst-%u",
    //          DBPath, rocks->hostname, worker_id, inst);
    // rocksdb_checkpoint_create(rocks->cp[worker_id], cp_path, log_size_for_flush,
    //                           &err);
    // if (err != NULL) {
    //   printf("Checkpoint Error: %s\n", err);
    // }
        send_checkpoint_message(worker_id, inst);
    }
}

static void stat_cb(__rte_unused struct rte_timer *timer,
                    __rte_unused void *arg) {
  uint32_t lcore = 0;
  uint64_t total_pkts = 0, total_bytes = 0;
  uint32_t i;

  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;

    if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
      continue;
    }

    total_pkts += lp->total_pkts;
    total_bytes += lp->total_bytes;
    lp->total_pkts = 0;
    lp->total_bytes = 0;
  }
  printf("%-8s\t%-4u\t%-10u\t", "Stat", app.burst_size_io_rx_read,
         app.p4xos_conf.checkpoint_interval);

  struct rocksdb_params *rocks = (struct rocksdb_params *)arg;
  uint32_t delivered_count = 0;
  for (i = 0; i < rocks->num_workers; i++) {
    if (i == 0) {
      printf("%-10u", rocks->delivered_count[i]);
    } else {
      printf("\t%-10u", rocks->delivered_count[i]);
    }
    delivered_count += rocks->delivered_count[i];
    rocks->delivered_count[i] = 0;
  }
  rocks->total_delivered_count += delivered_count;
  printf("\t%-10" PRIu64 "\t%-2.7f"
         "\t%-10u\n",
         total_pkts, bytes_to_gbits(total_bytes), delivered_count);

  if (rocks->total_delivered_count >= app.p4xos_conf.max_inst)
    app.force_quit = 1;
}

int main(int argc, char **argv) {
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
  argc -= ret;
  argv += ret;
  /* Parse application arguments (after the EAL ones) */
  ret = parse_rocksdb_configuration(argc, argv);
  if (ret < 0) {
    rocksdb_print_usage();
    return -1;
  }
  rocks.num_workers = app_get_lcores_worker();
  /* Init */
  app_init();
  app_init_learner();
  app_init_acceptor();
  app_print_params();
  print_parameters();

  init_rocksdb(&rocks);
  if (app.p4xos_conf.baseline)
    app_set_deliver_callback(baseline_deliver, &rocks);
  else
    app_set_deliver_callback(deliver, &rocks);

  app_set_worker_callback(replica_handler);
  app_set_stat_callback(stat_cb, &rocks);

  uint32_t i;
  uint32_t n_workers = app_get_lcores_worker();

  printf("%-8s\t%-4s\t%-10s\t", "Stats", "bsz", "cpi");
  printf("core%-6d", 0);
  for (i = 1; i < n_workers; i++) {
    printf("\tcore%-6d", i);
  }
  printf("\t%-10s\t%-10s\t%-10s\n", "packets", "Gbits", "throughput");

  /* Launch per-lcore init on every lcore */
  rte_eal_mp_remote_launch(app_lcore_main_loop, NULL, CALL_MASTER);
  RTE_LCORE_FOREACH_SLAVE(lcore) {
    if (rte_eal_wait_lcore(lcore) < 0) {
      return -1;
    }
  }

  return 0;
}
