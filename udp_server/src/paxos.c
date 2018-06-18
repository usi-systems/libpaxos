#include <stdio.h>
#include <arpa/inet.h>
#include "paxos.h"

void print_paxos_hdr(struct paxos_hdr* paxos) {
    printf("Msgtype %u, inst %u log_index %u, rnd %u vrnd %u worker_id %u\n",
    paxos->msgtype, paxos->inst, paxos->log_index, paxos->rnd, paxos->vrnd, paxos->worker_id);
}


void serialize_paxos_hdr(struct paxos_hdr* paxos) {
    paxos->inst = htonl(paxos->inst);
    paxos->log_index = htons(paxos->log_index);
    paxos->rnd = htons(paxos->rnd);
    paxos->vrnd = htons(paxos->vrnd);
    paxos->acptid = htons(paxos->acptid);
}

void deserialize_paxos_hdr(struct paxos_hdr* paxos) {
    paxos->inst = ntohl(paxos->inst);
    paxos->log_index = ntohs(paxos->log_index);
    paxos->rnd = ntohs(paxos->rnd);
    paxos->vrnd = ntohs(paxos->vrnd);
    paxos->acptid = ntohs(paxos->acptid);
}
