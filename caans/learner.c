#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "netpaxos.h"
#include "netutils.h"
#include "configuration.h"
#include "application_proxy.h"

void deliver(unsigned int inst, char* val, size_t size, void* arg) {
    struct application_ctx *app = arg;
    printf("DELIVERED: %d %s\n", inst, val);
    // struct application_ctx *app = arg;
    int *p = (int *) val;
    int proposer_id = ntohl(*p);
    p = (int *) (val + 4);
    int request_id = ntohl(*p);
    printf("proposer %d, request %d\n", proposer_id, request_id);
    int n = sendto(app->paxos->sock, val, size, 0,
                    (struct sockaddr *)&app->proxies[proposer_id],
                    sizeof(app->proxies[proposer_id]));
    if (n < 0)
        perror("deliver: sendto error");
}

void usage(char *prog) {
    printf("Usage: %s configuration-file\n", prog);
}


int main(int argc, char *argv[]) {

    if (argc < 2) {
        usage(argv[0]);
        return 0;
    }

    struct netpaxos_configuration conf;
    populate_configuration(argv[1], &conf);
    dump_configuration(&conf);

    struct application_ctx *app = malloc( sizeof (struct application_ctx));
    
    app->proxies = calloc(conf.proposer_count, sizeof(struct sockaddr_in));
    int i;
    for (i = 0; i < conf.proposer_count; i++) {
        ip_to_sockaddr( conf.proposer_address[i],
                    conf.proposer_port[i],
                    &app->proxies[i] );
    }
    struct paxos_ctx *paxos = make_learner(&conf, deliver, app);
    app->paxos = paxos;

    start_paxos(app->paxos);

    free_paxos_ctx(app->paxos);
    free(app->proxies);
    free(app);
    free_configuration(&conf);

    printf("Exit properly\n");
    return 0;
}