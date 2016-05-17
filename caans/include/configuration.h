#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include "netpaxos.h"

void dump_configuration(struct netpaxos_configuration *conf);
int populate_configuration(char* config, struct netpaxos_configuration *conf);
void free_configuration(struct netpaxos_configuration *conf);

#endif