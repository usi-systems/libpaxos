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

static void
stat_cb(__rte_unused struct rte_timer *timer, __rte_unused void *arg)
{
	uint32_t lcore;
	uint64_t total_pkts = 0, total_bytes = 0;
	for (lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;

		if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
			continue;
		}

		total_pkts += lp->total_pkts;
		total_bytes += lp->total_bytes;
		lp->total_pkts = 0;
		lp->total_bytes = 0;
	}

	if (total_pkts > 0) {
		printf("Throughput = %"PRIu64" pkts, %2.1f Gbits\n",
			   total_pkts, bytes_to_gbits(total_bytes));
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

	ret = rte_eal_hpet_init(1);
    if (ret < 0)
            rte_exit(EXIT_FAILURE, "Error with EAL HPET initialization\n");

	/* Init */
	app_init();
	app_init_leader();
	app_print_params();
	app_set_worker_callback(leader_handler);
	app_set_stat_callback(stat_cb, NULL);
	/* Launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(app_lcore_main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore) {
		if (rte_eal_wait_lcore(lcore) < 0) {
			return -1;
		}
	}

	return 0;
}
