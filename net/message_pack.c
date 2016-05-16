#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "message_pack.h"


void pack_paxos_accept(char* p, paxos_accept* v)
{
    size_t siz16 = sizeof(uint16_t);
    size_t siz32 = sizeof(uint32_t);

    uint16_t *typ = (uint16_t *)p;
    *typ = htons(PAXOS_ACCEPT);
    uint32_t *iid = (uint32_t *)(p + siz16);
    *iid = htonl(v->iid);
    uint16_t *ballot = (uint16_t *)(p + siz16 + siz32);
    *ballot = htons(v->ballot);
    uint16_t *value_ballot = (uint16_t *)(p + 2*siz16 + siz32);
    *value_ballot = htons(v->value_ballot);
    uint16_t *aid = (uint16_t *)(p + 3*siz16 + siz32);
    *aid = htons(v->aid);
    uint32_t *vsize = (uint32_t *)(p + 4*siz16 + siz32);
    *vsize = htonl(v->value.paxos_value_len);
    char *value =  (char *)(p + 4*siz16 + 2*siz32);
    memcpy(value, v->value.paxos_value_val, v->value.paxos_value_len);
}

void unpack_paxos_accept(paxos_accept* v, char* p)
{
    size_t siz16 = sizeof(uint16_t);
    size_t siz32 = sizeof(uint32_t);

    uint32_t *iid = (uint32_t *)(p + siz16);
    v->iid = ntohl(*iid);
    uint16_t *ballot = (uint16_t *)(p + siz16 + siz32);
    v->ballot = ntohs(*ballot);
    uint16_t *value_ballot = (uint16_t *)(p + 2*siz16 + siz32);
    v->value_ballot = ntohs(*value_ballot);
    uint16_t *aid = (uint16_t *)(p + 3*siz16 + siz32);
    v->aid = ntohs(*aid);
    uint32_t *vsize = (uint32_t *)(p + 4*siz16 + siz32);
    v->value.paxos_value_len = ntohl(*vsize);
    char *value =  (char *)(p + 4*siz16 + 2*siz32);

    v->value.paxos_value_val = malloc(v->value.paxos_value_len + 1);
    memcpy(v->value.paxos_value_val, value, v->value.paxos_value_len);
    v->value.paxos_value_val[v->value.paxos_value_len] = '\0';
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
