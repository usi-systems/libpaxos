#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "message_pack.h"

/* TODO: compute offsets */
#define IID_OFFSET 2
#define BALLOT_OFFSET 6
#define VALUE_BALLOT_OFFSET 8
#define AID_OFFSET 10
#define VSIZE_OFFSET 12
#define VALUE_OFFSET 16


void pack_message_type(char *p, uint16_t paxos_type) {
    uint16_t *typ = (uint16_t *)p;
    *typ = htons(paxos_type);
}

void pack_instance(char *p, uint32_t inst) {
    uint32_t *iid = (uint32_t *)(p + IID_OFFSET);
    *iid = htonl(inst);
}

void pack_ballot(char *p, uint16_t ballot_in) {
    uint16_t *ballot = (uint16_t *)(p + BALLOT_OFFSET);
    *ballot = htons(ballot_in);
}

void pack_value_ballot(char *p, uint16_t value_ballot_in) {
    uint16_t *value_ballot = (uint16_t *)(p + VALUE_BALLOT_OFFSET);
    *value_ballot = htons(value_ballot_in);
}

void pack_acceptor_id(char *p, uint16_t acceptor_id) {
    uint16_t *aid = (uint16_t *)(p + AID_OFFSET);
    *aid = htons(acceptor_id);
}

void pack_value(char *p, paxos_value *v) {
    uint32_t *vsize = (uint32_t *)(p + VSIZE_OFFSET);
    *vsize = htonl(v->paxos_value_len);
    char *value =  (char *)(p + VALUE_OFFSET);
    memcpy(value, v->paxos_value_val, v->paxos_value_len);
}

void unpack_instance(uint32_t *out_iid, char *p) {
    uint32_t *iid = (uint32_t *)(p + IID_OFFSET);
    *out_iid = ntohl(*iid);
}

void unpack_ballot(uint16_t *out_ballot, char *p) {
    uint16_t *ballot = (uint16_t *)(p + BALLOT_OFFSET);
    *out_ballot = ntohs(*ballot);
}

void unpack_value_ballot(uint16_t *out_value_ballot, char *p) {
    uint16_t *ballot = (uint16_t *)(p + VALUE_BALLOT_OFFSET);
    *out_value_ballot = ntohs(*ballot);
}

void unpack_acceptor_id(uint16_t *out_aid, char *p) {
    uint16_t *aid = (uint16_t *)(p + AID_OFFSET);
    *out_aid = htons(*aid);
}

void unpack_value(paxos_value *v, char *p) {
    uint32_t *vsize = (uint32_t *)(p + VSIZE_OFFSET);
    v->paxos_value_len = ntohl(*vsize);
    char *value =  (char *)(p + VALUE_OFFSET);
    v->paxos_value_val = malloc(v->paxos_value_len + 1);
    memcpy(v->paxos_value_val, value, v->paxos_value_len);
    v->paxos_value_val[v->paxos_value_len] = '\0';
}


void pack_paxos_accept(char* p, paxos_accept* v)
{
    pack_message_type(p, PAXOS_ACCEPT);
    pack_instance(p, v->iid);
    pack_ballot(p, v->ballot);
    pack_value_ballot(p, v->value_ballot);
    pack_acceptor_id(p, v->aid);
    pack_value(p, &v->value);
}

void unpack_paxos_accept(paxos_accept* v, char* p)
{
    unpack_instance(&v->iid, p);
    unpack_ballot(&v->ballot, p);
    unpack_value_ballot(&v->value_ballot, p);
    unpack_acceptor_id(&v->aid, p);
    unpack_value(&v->value, p);
}

void pack_paxos_message(char* p, paxos_message* v) {
    switch (v->type) {
    case PAXOS_ACCEPT:
        pack_paxos_accept(p, &v->u.accept);
        break;
    default:
        break;
    }
}

void unpack_paxos_message(paxos_message* v, char* p) {
    uint16_t *typ = (uint16_t *)p;
    v->type = ntohs(*typ);
    switch (v->type) {
    case PAXOS_ACCEPT:
        unpack_paxos_accept(&v->u.accept, p);
        break;
    default:
        break;
    }
}
