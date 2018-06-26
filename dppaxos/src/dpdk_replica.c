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
#include <signal.h>

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
  rocks->worker[worker_id].delivered_count++;
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
        uint32_t key_len = KEYLEN;   // rte_be_to_cpu_32(ap->key_len);
        uint32_t value_len = VALLEN; // rte_be_to_cpu_32(ap->value_len);
        // printf("Key %s, Value %s\n", ap->key, ap->value);
        // // Single PUT
        handle_put(rocks->worker[worker_id].db, rocks->writeoptions, (const char *)&ap->req.write.key,
                    key_len, (const char *)&ap->req.write.value, value_len);
        rocks->worker[worker_id].write_count++;

    } else if (ap->type == READ_REQ) {
        size_t len;
        uint32_t key_len = KEYLEN; // rte_be_to_cpu_32(ap->key_len);
        // printf("Key %s\n", ap->key);
        char *returned_value =
        handle_get(rocks->worker[worker_id].db, rocks->readoptions, (const char *)&ap->req.read.key, key_len, &len);
        if (returned_value != NULL) {
            // printf("Key %s: return value %s\n", ap->key, returned_value);
            rte_memcpy((char *)&ap->req.read.value, returned_value, len);
            free(returned_value);
        }
        rocks->worker[worker_id].read_count++;
    } else if (ap->type == BACKUP_REQ) {
        handle_backup(rocks->worker[worker_id].db, rocks->worker[worker_id].be);
    }

    rocks->worker[worker_id].delivered_count++;

    if (inst > 0 && app.p4xos_conf.checkpoint_interval > 0 &&
      (inst % app.p4xos_conf.checkpoint_interval == 0)) {
        if (rocksdb_configurations.enable_checkpoint) {
            char cp_path[FILENAME_LENGTH];
            snprintf(cp_path, FILENAME_LENGTH, "%s/checkpoints/%s-core-%u-inst-%u",
            rocks->worker[worker_id].db_path, rocks->hostname, worker_id, inst);
            handle_checkpoint(rocks->worker[worker_id].cp, cp_path);
        }
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
      printf("%-10u", rocks->worker[i].delivered_count);
    } else {
      printf("\t%-10u", rocks->worker[i].delivered_count);
    }
    delivered_count += rocks->worker[i].delivered_count;
    rocks->worker[i].delivered_count = 0;
  }
  rocks->total_delivered_count += delivered_count;
  printf("\t%-10" PRIu64 "\t%-2.7f"
         "\t%-10u\n",
         total_pkts, bytes_to_gbits(total_bytes), delivered_count);

  if (rocks->total_delivered_count >= app.p4xos_conf.max_inst)
    app.force_quit = 1;
}


static inline size_t
get_file_size(FILE *fp)
{
    fseek(fp, 0L, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    return sz;
}


int recovery_cb(char *buffer, size_t len, uint32_t worker_id, struct sockaddr_in *from)
{
    struct request *ap = (struct request*)buffer;
    printf("Request type %u\n", ap->type);
    if (ap->type == BACKUP_REQ) {
        handle_backup(rocks.worker[worker_id].db, rocks.worker[worker_id].be);
        char filename[] = "/tmp/backup.tar";
        char cmd[256];
        snprintf(cmd, 256, "tar cvf %s %s/backup", filename, rocksdb_configurations.db_paths[worker_id]);
        int ret = system(cmd);
        if (ret < 0) {
            printf("Cannot create tar file for backup\n");
            return -1;
        }
        #define MAXBUFLEN 1024
        char data[MAXBUFLEN];
        FILE *fp = fopen(filename, "r");
        if (fp == NULL) {
            RTE_LOG(WARNING, P4XOS, "Open file %s has errors\n", filename);
            return -1;
        }
        size_t data_len = fread(data, sizeof(char), MAXBUFLEN, fp);
        while (data_len > 0)
        {
            net_sendto(worker_id, data, data_len, &app.p4xos_conf.primary_replica);
            data_len = fread(data, sizeof(char), MAXBUFLEN, fp);
        }
        fclose(fp);

        return 0;
    }
    else if (ap->type == BACKUP_RES) {
        uint32_t buffer_size = rte_be_to_cpu_32(ap->req.backup_res.bufsize);

        char filename[] = "/tmp/backup.tar";
        FILE *fp = fopen(filename,"ab");
        if (fp) {
            fwrite(ap->req.backup_res.buffer, buffer_size, 1, fp);
            printf("Write %u bytes to file %s\n", buffer_size, filename);
        }
        fclose(fp);
        return -1;
    }
    return 0;
}

static void
int_handler(int sig_num)
{
	printf("Exiting on signal %d\n", sig_num);
	/* set quit flag for thread to exit */
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

  /* catch ctrl-c so we can print on exit */
  signal(SIGINT, int_handler);
  signal(SIGKILL, int_handler);

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

  app_set_register_cb(app.p4xos_conf.primary_replica.sin_port, recovery_cb);
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
  cleanup(&rocks);
  return 0;
}
