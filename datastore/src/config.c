#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include "datastore.h"

#define DEFAULT_CONFIG_FILE "rocksdb.conf"

static const char usage[] =
    "    rocksdb_bench <EAL PARAMS> -- <APP PARAMS>                          \n"
    "                                                                        \n"
    "Application optional parameters:                                        \n"
    "    --write_sync : Enable write sync mode (default value is %u)         \n"
    "    --disable_wal : Disable WAL mode (default value is %u)              \n"
    "    --enable_checkpoint : Enable checkpoint mode (default value is %u)  \n"
    "    --fsync : Enable fsync mode (default value is %u)                   \n"
    "    --mem_budget : set memory budge (default value is %u)               \n"
    "    --config_file : set config_file (default value is %s)               \n"
    "    --flush_size : set log size for flush (default value is %u)         "
    "\n";

void rocksdb_print_usage(void) {
  printf(usage, DEFAULT_WRITE_SYNC, DEFAULT_DISABLE_WAL, DEFAULT_DISABLE_CHECKPOINT,
         DEFAULT_DISABLE_FSYNC, DEFAULT_MEM_BUDGET, DEFAULT_CONFIG_FILE,
         DEFAULT_LOG_SIZE_FLUSH);
}

static int parse_arg_uint32(const char *arg, uint32_t *out_number) {
  uint32_t x;
  char *endpt;

  if (strnlen(arg, APP_ARG_NUMERICAL_SIZE_CHARS + 1) ==
      APP_ARG_NUMERICAL_SIZE_CHARS + 1) {
    return -1;
  }

  errno = 0;
  x = strtoul(arg, &endpt, 10);
  if (errno != 0 || endpt == arg || *endpt != '\0') {
    return -2;
  }

  *out_number = (uint32_t)x;
  return 0;
}


void populate_configuration(char *config_file, struct rocksdb_configurations *conf) {
    FILE *fp = fopen(config_file, "r");
    if (fp == NULL)
        return;

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    const char delim[2] = " ";

    while ((read = getline(&line, &len, fp)) != -1) {
        char *token;
        token = strtok(line, delim);
        while ( token != NULL) {
            if (strcmp(token, "#") == 0) {
                break;
            }
            else if (strcmp(token, "write_sync") == 0) {
                token = strtok(NULL, delim);
                if (conf->write_sync == DEFAULT_WRITE_SYNC) {
                    conf->write_sync = atoi(token);
                }
            }
            else if (strcmp(token, "use_fsync") == 0) {
                token = strtok(NULL, delim);
                if (conf->use_fsync == DEFAULT_DISABLE_FSYNC) {
                    conf->use_fsync = atoi(token);
                }
            }
            else if (strcmp(token, "disable_wal") == 0) {
                token = strtok(NULL, delim);
                if (conf->disable_wal == DEFAULT_DISABLE_WAL) {
                    conf->disable_wal = atoi(token);
                }
            }
            else if (strcmp(token, "rm_existed_db") == 0) {
                token = strtok(NULL, delim);
                if (conf->rm_existed_db == DEFAULT_RM_EXISTED_DB) {
                    conf->rm_existed_db = atoi(token);
                }
            }
            else if (strcmp(token, "sync_db") == 0) {
                token = strtok(NULL, delim);
                if (conf->sync_db == DEFAULT_SYNC_DB) {
                    conf->sync_db = atoi(token);
                }
            }
            else if (strcmp(token, "enable_checkpoint") == 0) {
                token = strtok(NULL, delim);
                if (conf->enable_checkpoint == DEFAULT_DISABLE_CHECKPOINT) {
                    conf->enable_checkpoint = atoi(token);
                }
            }
            else if (strcmp(token, "enable_statistics") == 0) {
                token = strtok(NULL, delim);
                if (conf->enable_statistics == DEFAULT_DISABLE_STATISTICS) {
                    conf->enable_statistics = atoi(token);
                }
            }
            else if (strcmp(token, "flush_size") == 0) {
                token = strtok(NULL, delim);
                if (conf->flush_size == DEFAULT_LOG_SIZE_FLUSH) {
                    conf->flush_size = atoi(token);
                }
            }
            else if (strcmp(token, "nb_keys") == 0) {
                token = strtok(NULL, delim);
                if (conf->nb_keys == DEFAULT_NB_KEYS) {
                    conf->nb_keys = atoi(token);
                }
            }
            else if (strcmp(token, "db_paths") == 0) {
                token = strtok(NULL, delim);
                while(token != NULL && strlen(token) > 0) {
                    conf->db_paths[conf->partition_count] = strdup(token);
                    conf->partition_count++;
                    token = strtok(NULL, delim);
                }
            }
            else if (strcmp(token, "replica_hostname") == 0) {
                token = strtok(NULL, delim);
                if (token != NULL && strlen(token) > 0) {
                    conf->replica_hostname = strdup(token);
                    token = strtok(NULL, delim);
                }
            }
            if (token != NULL)
                token = strtok(NULL, delim);
        }
    }

    fclose(fp);
}


/* Parse the argument given in the command line of the application */
int parse_rocksdb_configuration(int argc, char **argv) {
    int opt, ret;
    char **argvopt;
    int option_index;
    char *prgname = argv[0];
    static struct option lgopts[] = {
        {"write_sync", 0, 0, 0},  {"fsync", 0, 0, 0},
        {"enable_checkpoint", 0, 0, 0}, {"disable_wal", 0, 0, 0},
        {"mem_budget", 1, 0, 0}, {"enable-statistics", 0, 0, 0},
        {"config_file", 1, 0, 0}, {"flush_size", 1, 0, 0},
        {"nb_keys", 1, 0, 0}, {NULL, 0, 0, 0}};

    rocksdb_configurations.write_sync = DEFAULT_WRITE_SYNC;
    rocksdb_configurations.disable_wal = DEFAULT_DISABLE_WAL;
    rocksdb_configurations.enable_checkpoint = DEFAULT_DISABLE_CHECKPOINT;
    rocksdb_configurations.use_fsync = DEFAULT_DISABLE_FSYNC;
    rocksdb_configurations.mem_budget = DEFAULT_MEM_BUDGET;
    rocksdb_configurations.flush_size = DEFAULT_LOG_SIZE_FLUSH;
    rocksdb_configurations.enable_statistics = DEFAULT_DISABLE_STATISTICS;
    rocksdb_configurations.nb_keys = DEFAULT_NB_KEYS;

    char* arg_config_file = NULL;

    argvopt = argv;

    while ((opt = getopt_long(argc, argvopt, "", lgopts, &option_index)) != EOF) {
        switch (opt) {
        /* long options */
        case 0:
        if (!strcmp(lgopts[option_index].name, "write_sync")) {
            rocksdb_configurations.write_sync = 1;
        } else if (!strcmp(lgopts[option_index].name, "fsync")) {
            rocksdb_configurations.use_fsync = 1;
        } else if (!strcmp(lgopts[option_index].name, "enable_checkpoint")) {
            rocksdb_configurations.enable_checkpoint = 1;
        } else if (!strcmp(lgopts[option_index].name, "enable-statistics")) {
            rocksdb_configurations.enable_statistics = 1;
        } else if (!strcmp(lgopts[option_index].name, "disable_wal")) {
            rocksdb_configurations.disable_wal = 1;
        } else if (!strcmp(lgopts[option_index].name, "mem_budget")) {
            ret = parse_arg_uint32(optarg, &rocksdb_configurations.mem_budget);
            if (ret) {
                printf("Incorrect value for --mem_budget argument (%d)\n", ret);
                return -1;
            }
        } else if (!strcmp(lgopts[option_index].name, "flush_size")) {
            ret = parse_arg_uint32(optarg, &rocksdb_configurations.flush_size);
            if (ret) {
                printf("Incorrect value for --flush_size argument (%d)\n", ret);
                return -1;
            }
        } else if (!strcmp(lgopts[option_index].name, "nb_keys")) {
            ret = parse_arg_uint32(optarg, &rocksdb_configurations.nb_keys);
            if (ret) {
                printf("Incorrect value for --nb_keys argument (%d)\n", ret);
                return -1;
            }
        }
        else if (!strcmp(lgopts[option_index].name, "config_file")) {
            arg_config_file = strdup(optarg);
       }
        break;
        default:
            return -1;
        }
    }

    if (arg_config_file == NULL) {
        arg_config_file = strdup(DEFAULT_CONFIG_FILE);
    }
    populate_configuration(arg_config_file, &rocksdb_configurations);
    free(arg_config_file);
    if (optind >= 0)
        argv[optind - 1] = prgname;

    ret = optind - 1;
    optind = 1; /* reset getopt lib */
    return ret;
}

void free_rocksdb_configurations(struct rocksdb_configurations *conf) {
    unsigned i;
    for (i = 0; i < conf->partition_count; i++)
        free(conf->db_paths[i]);

    if (conf->replica_hostname) {
        free(conf->replica_hostname);
    }
}


void print_parameters(void) {
    printf("Used paramenters\n"
        "write_sync: %u\n"
        "disable_wal: %u\n"
        "use-fsync: %u\n"
        "enable_checkpoint: %u\n"
        "rm_existed_db: %u\n"
        "sync_db: %u\n"
        "enable-statistics: %u\n"
        "mem_budget: %u\n"
        "nb_keys: %u\n"
        "flush_size: %u\n"
        "replica_hostname: %s\n",
        rocksdb_configurations.write_sync, rocksdb_configurations.disable_wal,
        rocksdb_configurations.use_fsync, rocksdb_configurations.enable_checkpoint,
        rocksdb_configurations.rm_existed_db, rocksdb_configurations.sync_db,
        rocksdb_configurations.enable_statistics, rocksdb_configurations.mem_budget,
        rocksdb_configurations.nb_keys, rocksdb_configurations.flush_size,
        rocksdb_configurations.replica_hostname);

    unsigned i;
    for (i = 0; i < rocksdb_configurations.partition_count; i++)
        printf("Partition %u db_path %s\n", i, rocksdb_configurations.db_paths[i]);

}
