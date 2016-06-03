#ifndef APPLICATION_CONFIG_H
#define APPLICATION_CONFIG_H

#include "net_utils.h"

#define MAX_LEARNER 2

typedef struct {
    int number_of_learners;
    struct address learners[MAX_LEARNER];
} application_config;

application_config *parse_configuration(const char *config_file);
void free_application_config(application_config *conf);
#endif