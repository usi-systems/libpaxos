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


static uint8_t DEFAULT_KEY[] = "ABCDEFGH1234567";
static uint8_t DEFAULT_VALUE[] = "ABCDEFGH1234567";

static void
set_app_hdr(struct app_hdr *ap, uint32_t inst) {
	ap->msg_type = (inst % 2);
	ap->key_len = rte_cpu_to_be_32(sizeof(DEFAULT_KEY));
	ap->value_len = rte_cpu_to_be_32(sizeof(DEFAULT_VALUE));
	rte_memcpy(ap->key, DEFAULT_KEY, sizeof(DEFAULT_KEY));
	if (ap->msg_type == WRITE_OP) {
		rte_memcpy(ap->value, DEFAULT_VALUE, sizeof(DEFAULT_VALUE));
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

	// ret = rte_eal_hpet_init(1);
    // if (ret < 0)
    //         rte_exit(EXIT_FAILURE, "Error with EAL HPET initialization\n");

	uint64_t cycles_per_s = rte_get_timer_hz();
	printf("Cycles per s: %lu", cycles_per_s);
	/* Init */
	app_init();
	app_print_params();
	app_set_worker_callback(proposer_handler);
	struct app_hdr ap;
	uint32_t i;
	for (i = 0; i < app.p4xos_conf.osd; i++) {
		set_app_hdr(&ap, i);
		submit((char*)&ap, sizeof(struct app_hdr));
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
