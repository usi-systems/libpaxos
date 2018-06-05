#include "stats.h"

void init_app_stats(struct app_stats *app_stats, char *latency_fn) {
    app_stats->latency_fp = fopen(latency_fn, "w");
    app_stats->buffer_count = 0;
}

void write_latency_to_file(struct app_stats *app_stats, double latency) {
    app_stats->buffer_count +=
        sprintf(&app_stats->file_buffer[app_stats->buffer_count], "%.0f\n", latency);
    if (app_stats->buffer_count >= CHUNK_SIZE) {
        fwrite(app_stats->file_buffer, app_stats->buffer_count, 1, app_stats->latency_fp);
        app_stats->buffer_count = 0;
    }
}
