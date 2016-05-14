#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "application_config.h"


void free_application_config(application_config *config) {
    int i;
    for (i = 0; i < config->number_of_learners; i++) {
        if (config->learners[i].addr != NULL) {
            free(config->learners[i].addr);
        }
    }
    free(config);
}

application_config *parse_configuration(const char *config_file) {
    application_config *conf = malloc(sizeof(application_config));
    conf->number_of_learners = 0;

    FILE *fp = fopen(config_file, "r");
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    if (fp == NULL)
        exit(EXIT_FAILURE);

    while ((read = getline(&line, &len, fp)) != -1) {
        const char delim[2] = " ";
        char *token;
        token = strtok(line, delim);
        while ( token != NULL) {
            if (strcmp(token, "learner") == 0) {
                int idx = conf->number_of_learners;
                token = strtok(NULL, delim);
                conf->learners[idx].addr = strdup(token);
                token = strtok(NULL, delim);
                conf->learners[idx].port = atoi(token);
                conf->number_of_learners++;
            }
            token = strtok(NULL, delim);
        }
    }
    fclose(fp);
    if (line)
        free(line);
    return conf;
}