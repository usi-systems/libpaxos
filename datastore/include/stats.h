#ifndef _STATS_H_
#define _STATS_H_

#include <stdio.h>
#include <inttypes.h>

#define CHUNK_SIZE 4096

struct app_stats {
    FILE *latency_fp;
    uint32_t buffer_count;
    char file_buffer[CHUNK_SIZE + 64];
};

void init_app_stats(struct app_stats *app_stats, char *latency_fn);
void write_latency_to_file(struct app_stats *app_stats, double latency);


#endif
