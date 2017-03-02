#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "configuration.h"

int parse_verbosity(char* str, paxos_log_level* verbosity);

void dump_configuration(struct netpaxos_configuration *conf)
{
    int i;
    printf ("--------------------------\n");
    for (i = 0; i < conf->learner_count; i++) {
        paxos_log_debug("learner thread %d: %s %d", i, conf->learner_address[i], conf->learner_port[i]);
    }
    //paxos_log_debug("learner: %s %d", conf->learner_address, conf->learner_port);
    paxos_log_debug("acceptor: %s %d", conf->acceptor_address, conf->acceptor_port);
    paxos_log_debug("num_acceptors: %d", conf->acceptor_count);
   
    for (i = 0; i < conf->proposer_count; i++) {
        paxos_log_debug("proposer %d: %s %d", i, conf->proposer_address[i], conf->proposer_port[i]);
    }
   printf ("--------------------------\n");
}

int populate_configuration(char* config, struct netpaxos_configuration *conf)
{
    //conf->learner_port = 0;
    //conf->learner_address =NULL;
    conf->acceptor_port = 0;
    conf->acceptor_address = NULL;
    conf->acceptor_count = 1;
    conf->proxy_port = 0;
    conf->proposer_port = 0;
    conf->proposer_address = NULL;
    conf->coordinator_port = 0;
    conf->coordinator_address = NULL;
    /* Initialize number of proposers to 10 */
    conf->proposer_count = 0;
    conf->max_num_proposer = 10;
    conf->proposer_port = calloc(conf->max_num_proposer, sizeof(int));
    conf->proposer_address = calloc(conf->max_num_proposer, sizeof(char*));
    int i;
    for (i = 0; i < conf->max_num_proposer; i++) {
        conf->proposer_port[i] = 0;
        conf->proposer_address[i] = NULL;
    }

    conf->learner_count = 0;
    conf->max_num_learner_thread = 12;
    conf->learner_port = calloc(conf->max_num_learner_thread, sizeof(int));
    conf->learner_address =  calloc(conf->max_num_learner_thread, sizeof(char*));
    int j;
    for (j = 0; j < conf->max_num_learner_thread; j++) {
        conf->learner_port[j] = 0;
        conf->learner_address[j] = NULL;
    }
    FILE *fp = fopen(config, "r");
    if (fp == NULL)
        return EXIT_FAILURE;

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    const char delim[2] = " ";


    while ((read = getline(&line, &len, fp)) != -1) {
        char *token;
        token = strtok(line, delim);
        while ( token != NULL) {
            if (strcmp(token, "proposer") == 0) {
                token = strtok(NULL, delim);
                conf->proposer_address[conf->proposer_count] = strdup(token);
                token = strtok(NULL, delim);
                conf->proposer_port[conf->proposer_count] = atoi(token);
                conf->proposer_count++;
            }
            if (strcmp(token, "acceptor") == 0) {
                token = strtok(NULL, delim);
                conf->acceptor_address = strdup(token);
                token = strtok(NULL, delim);
                conf->acceptor_port = atoi(token);
            }
            if (strcmp(token, "learner") == 0) {
                /*token = strtok(NULL, delim);
                conf->learner_address = strdup(token);
                token = strtok(NULL, delim);
                conf->learner_port = atoi(token);*/
                token = strtok(NULL, delim);
                conf->learner_address[conf->learner_count] = strdup(token);
                token = strtok(NULL, delim);
                conf->learner_port[conf->learner_count] = atoi(token);
                conf->learner_count++;
            }
            if (strcmp(token, "coordinator") == 0) {
                token = strtok(NULL, delim);
                conf->coordinator_address = strdup(token);
                token = strtok(NULL, delim);
                conf->coordinator_port = atoi(token);
            }
            if (strcmp(token, "num_acceptors") == 0) {
                token = strtok(NULL, delim);
                conf->acceptor_count = atoi(token);
            }
            if (strcmp(token, "proxy_port") == 0) {
                token = strtok(NULL, delim);
                conf->proxy_port = atoi(token);
            }
            if (strcmp(token, "proposer_preexec_window") == 0) {
                token = strtok(NULL, delim);
                paxos_config.proposer_preexec_window = atoi(token);
            }
            if (strcmp(token, "verbosity") == 0) {
                token = strtok(NULL, delim);
                parse_verbosity(token, &paxos_config.verbosity);
            }
            token = strtok(NULL, delim);
        }
    }
    fclose(fp);
    if (line)
        free(line);

    assert(conf->learner_address != NULL);
    assert(conf->acceptor_address != NULL);

    return EXIT_SUCCESS;
}

int parse_verbosity(char* str, paxos_log_level* verbosity)
{
    strtok(str, "\n");
    if (strcasecmp(str, "quiet") == 0) *verbosity = PAXOS_LOG_QUIET;
    else if (strcasecmp(str, "error") == 0) *verbosity = PAXOS_LOG_ERROR;
    else if (strcasecmp(str, "info") == 0) *verbosity = PAXOS_LOG_INFO;
    else if (strcasecmp(str, "debug") == 0) *verbosity = PAXOS_LOG_DEBUG;
    else return 0;
    return 1;
}

void free_configuration(struct netpaxos_configuration *conf)
{
    int i;
    for (i = 0; i < conf->max_num_proposer; i++) {
        if (conf->proposer_address[i] != NULL)
            free(conf->proposer_address[i]);
    }
    free(conf->proposer_port);
    free(conf->proposer_address);
    if (conf->learner_address)
        free(conf->learner_address);
    if (conf->acceptor_address)
        free(conf->acceptor_address);
    if (conf->coordinator_address)
        free(conf->coordinator_address);
}