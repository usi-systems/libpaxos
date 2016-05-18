#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "configuration.h"

void dump_configuration(struct netpaxos_configuration *conf)
{
    printf("learner: %s %d\n", conf->learner_address, conf->learner_port);
    printf("acceptor: %s %d\n", conf->acceptor_address, conf->acceptor_port);
    printf("num_acceptors: %d\n", conf->acceptor_count);
}

int populate_configuration(char* config, struct netpaxos_configuration *conf)
{
    conf->learner_port = 0;
    conf->learner_address =NULL;
    conf->acceptor_port = 0;
    conf->acceptor_address = NULL;
    conf->acceptor_count = 1;
    conf->proxy_port = 0;
    conf->proposer_port = 0;
    conf->proposer_address = NULL;
    /* Initialize number of proposers to 5 */
    conf->proposer_count = 0;
    conf->max_num_proposer = 5;
    conf->proposer_port = calloc(conf->max_num_proposer, sizeof(int));
    conf->proposer_address = calloc(conf->max_num_proposer, sizeof(char*));
    int i;
    for (i = 0; i < conf->max_num_proposer; i++) {
        conf->proposer_port[i] = 0;
        conf->proposer_address[i] = NULL;
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
                token = strtok(NULL, delim);
                conf->learner_address = strdup(token);
                token = strtok(NULL, delim);
                conf->learner_port = atoi(token);
            }
            if (strcmp(token, "num_acceptors") == 0) {
                token = strtok(NULL, delim);
                conf->acceptor_count = atoi(token);
            }
            if (strcmp(token, "proxy_port") == 0) {
                token = strtok(NULL, delim);
                conf->proxy_port = atoi(token);
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
}