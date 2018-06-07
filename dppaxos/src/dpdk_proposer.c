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
#include "datastore.h"


struct rocksdb_configurations rocksdb_configurations;


static void
set_request(struct request *ap, uint32_t some_value) {
	ap->type = WRITE_REQ;
	rte_memcpy((char *)&ap->key, (char*)&some_value, 1);
	rte_memcpy((char *)&ap->value, (char*)&some_value, 1);
}

static void receive_response(unsigned int worker_id,
                             unsigned int __rte_unused inst,
                             __rte_unused char *val, __rte_unused size_t size,
                             __rte_unused void *arg) {
	struct request *req = (struct request*) val;
	// printf("Worker %u Received a response: %u %u %u\n", worker_id, req->type, req->key, req->value);
	req->type = WRITE_REQ;
	req->key = req->key + 1;
	req->value = req->value + 1;
}

static void
stat_cb(__rte_unused struct rte_timer *timer, __rte_unused void *arg)
{
	uint32_t lcore, nb_delivery = 0, nb_latency = 0;
	uint64_t latency = 0;
	uint64_t total_pkts = 0, total_bytes = 0;

	for (lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;

		if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
			continue;
		}
		printf("%-10"PRIu64, lp->nb_delivery);
		nb_latency += lp->nb_latency;
		nb_delivery += lp->nb_delivery;
		latency += lp->latency;
		total_pkts += lp->total_pkts;
		total_bytes += lp->total_bytes;
		lp->nb_latency = 0;
		lp->nb_delivery = 0;
		lp->latency = 0;
		lp->total_pkts = 0;
		lp->total_bytes = 0;
	}

	double avg_cycle_latency = 0.0, avg_ns_latency = 0.0;
	if (latency) {
		avg_cycle_latency = (double) latency / (double) nb_latency;
		avg_ns_latency = avg_cycle_latency * NS_PER_S / rte_get_timer_hz();
	}
	printf("Throughput = %"PRIu64" pkts, %2.1f Gbits; "
			"Paxos TP = %u Avg latency = %.2f cycles ~ %.1f ns\n",
			total_pkts, bytes_to_gbits(total_bytes),
			nb_delivery, avg_cycle_latency, avg_ns_latency);
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
	argc -= ret;
    argv += ret;
	/* Parse application arguments (after the EAL ones) */
    ret = parse_rocksdb_configuration(argc, argv);
    if (ret < 0) {
      rocksdb_print_usage();
      return -1;
    }
	// ret = rte_eal_hpet_init(1);
    // if (ret < 0)
    //         rte_exit(EXIT_FAILURE, "Error with EAL HPET initialization\n");

	uint64_t cycles_per_s = rte_get_timer_hz();
	printf("Cycles per s: %lu\n", cycles_per_s);
	/* Init */
	app_init();
	app_print_params();
	app_set_deliver_callback(receive_response, NULL);
	app_set_worker_callback(proposer_handler);
	app_set_stat_callback(stat_cb, NULL);
	app_init_proposer();

	uint32_t n_workers = app_get_lcores_worker();

	struct request req[app.p4xos_conf.osd];
	uint32_t i;
	for (i = 0; i < app.p4xos_conf.osd; i++) {
		set_request(&req[i], i+1);
	}
	uint32_t worker_id = 0;

	for (lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;

		if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
			continue;
		}
		submit_bulk(worker_id, app.p4xos_conf.osd, lp, (char*)req, sizeof(struct request));
		worker_id++;
		if (worker_id == n_workers)
			break;

	}

	/* Launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(app_lcore_main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		if (rte_eal_wait_lcore(lcore) < 0) {
			return -1;
		}
	}

	return 0;
}
