#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "message_pack.h"

/* TODO: compute offsets */
#define MSGTYPE_OFFSET 0
#define IID_OFFSET 2
#define BALLOT_OFFSET 6
#define VALUE_BALLOT_OFFSET 8
#define AID_OFFSET 10
#define VALUE_OFFSET 12
/* paxos_repeat offsets */
#define FROM_OFFSET 2
#define TO_OFFSET 6
/* paxos_trim offset */
#define TRIM_OFFSET 2
/* paxos_acceptor_state offsets */
#define STATE_AID_OFFSET 2
#define TRIM_IID_OFFSET 6


void pack_uint32(char *p, uint32_t value, int offset)
{
    uint32_t *serialize = (uint32_t *)(p + offset);
    *serialize = htonl(value);
}

void pack_uint16(char *p, uint16_t value, int offset)
{
    uint16_t *serialize = (uint16_t *)(p + offset);
    *serialize = htons(value);
}

void pack_value(char *p, paxos_value *v, int offset)
{
    pack_uint32(p, v->paxos_value_len, offset);
    char *value =  (char *)(p + offset + 4);
    memcpy(value, v->paxos_value_val, v->paxos_value_len);
}

void unpack_uint32(uint32_t *out_value, char *p, int offset)
{
    uint32_t *raw_bytes = (uint32_t *)(p + offset);
    *out_value = ntohl(*raw_bytes);
}

void unpack_uint16(uint16_t *out_value, char *p, int offset)
{
    uint16_t *raw_bytes = (uint16_t *)(p + offset);
    *out_value = ntohs(*raw_bytes);
}

void unpack_value(paxos_value *v, char *p, int offset)
{
    unpack_uint32((uint32_t *)&v->paxos_value_len, p, offset);
    char *value =  (char *)(p + offset + 4);
    v->paxos_value_val = malloc(v->paxos_value_len + 1);
    memcpy(v->paxos_value_val, value, v->paxos_value_len);
    v->paxos_value_val[v->paxos_value_len] = '\0';
}

void pack_paxos_prepare(char* p, paxos_prepare* v)
{
    pack_uint16(p, PAXOS_PREPARE, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
}

void unpack_paxos_prepare(paxos_prepare* v, char* p)
{
    unpack_uint32(&v->iid, p, IID_OFFSET);
    unpack_uint16(&v->ballot, p, BALLOT_OFFSET);
    unpack_uint16(&v->value_ballot, p, VALUE_BALLOT_OFFSET);
    unpack_uint16(&v->aid, p, AID_OFFSET);
    unpack_value(&v->value, p, VALUE_OFFSET);
}

void pack_paxos_promise(char* p, paxos_promise* v)
{
    pack_uint16(p, PAXOS_PROMISE, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
}

void unpack_paxos_promise(paxos_promise* v, char* p)
{
    unpack_uint32(&v->iid, p, IID_OFFSET);
    unpack_uint16(&v->ballot, p, BALLOT_OFFSET);
    unpack_uint16(&v->value_ballot, p, VALUE_BALLOT_OFFSET);
    unpack_uint16(&v->aid, p, AID_OFFSET);
    unpack_value(&v->value, p, VALUE_OFFSET);
}

void pack_paxos_accept(char* p, paxos_accept* v)
{
    pack_uint16(p, PAXOS_ACCEPT, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
}

void unpack_paxos_accept(paxos_accept* v, char* p)
{
    unpack_uint32(&v->iid, p, IID_OFFSET);
    unpack_uint16(&v->ballot, p, BALLOT_OFFSET);
    unpack_uint16(&v->value_ballot, p, VALUE_BALLOT_OFFSET);
    unpack_uint16(&v->aid, p, AID_OFFSET);
    unpack_value(&v->value, p, VALUE_OFFSET);
}

void pack_paxos_accepted(char* p, paxos_accepted* v)
{
    pack_uint16(p, PAXOS_ACCEPTED, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
}

void unpack_paxos_accepted(paxos_accepted* v, char* p)
{
    unpack_uint32(&v->iid, p, IID_OFFSET);
    unpack_uint16(&v->ballot, p, BALLOT_OFFSET);
    unpack_uint16(&v->value_ballot, p, VALUE_BALLOT_OFFSET);
    unpack_uint16(&v->aid, p, AID_OFFSET);
    unpack_value(&v->value, p, VALUE_OFFSET);
}

void pack_paxos_preempted(char* p, paxos_preempted* v)
{
    pack_uint16(p, PAXOS_PREEMPTED, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
}

void unpack_paxos_preempted(paxos_preempted* v, char* p)
{
    unpack_uint32(&v->iid, p, IID_OFFSET);
    unpack_uint16(&v->ballot, p, BALLOT_OFFSET);
    unpack_uint16(&v->value_ballot, p, VALUE_BALLOT_OFFSET);
    unpack_uint16(&v->aid, p, AID_OFFSET);
    unpack_value(&v->value, p, VALUE_OFFSET);
}

void pack_paxos_repeat(char* p, paxos_repeat* v)
{
    pack_uint16(p, PAXOS_REPEAT, MSGTYPE_OFFSET);
    pack_uint32(p, v->from, FROM_OFFSET);
    pack_uint32(p, v->to, TO_OFFSET);
}

void unpack_paxos_repeat(paxos_repeat* v, char* p)
{
    unpack_uint32(&v->from, p, FROM_OFFSET);
    unpack_uint32(&v->to, p, TO_OFFSET);
}

void pack_paxos_trim(char* p, paxos_trim* v)
{
    pack_uint16(p, PAXOS_TRIM, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, TRIM_OFFSET);
}

void unpack_paxos_trim(paxos_trim* v, char* p)
{
    unpack_uint32(&v->iid, p, TRIM_OFFSET);
}

void pack_paxos_acceptor_state(char* p, paxos_acceptor_state* v)
{
    pack_uint16(p, PAXOS_ACCEPTOR_STATE, MSGTYPE_OFFSET);
    pack_uint32(p, v->aid, STATE_AID_OFFSET);
    pack_uint32(p, v->trim_iid, TRIM_IID_OFFSET);
}

void unpack_paxos_acceptor_state(paxos_acceptor_state* v, char* p)
{
    unpack_uint32(&v->aid, p, STATE_AID_OFFSET);
    unpack_uint32(&v->trim_iid, p, TRIM_IID_OFFSET);
}

void pack_paxos_client_value(char* p, paxos_client_value* v)
{
    pack_value(p, &v->value, 2);
}

void unpack_paxos_client_value(paxos_client_value* v, char* p)
{
    unpack_value(&v->value, p, 2);
}

void pack_paxos_message(char* p, paxos_message* v)
{
    switch (v->type) {
    case PAXOS_PREPARE:
        pack_paxos_prepare(p, &v->u.prepare);
        break;
    case PAXOS_PROMISE:
        pack_paxos_promise(p, &v->u.promise);
        break;
    case PAXOS_ACCEPT:
        pack_paxos_accept(p, &v->u.accept);
        break;
    case PAXOS_ACCEPTED:
        pack_paxos_accepted(p, &v->u.accepted);
        break;
    case PAXOS_PREEMPTED:
        pack_paxos_preempted(p, &v->u.preempted);
        break;
    case PAXOS_REPEAT:
        pack_paxos_repeat(p, &v->u.repeat);
        break;
    case PAXOS_TRIM:
        pack_paxos_trim(p, &v->u.trim);
        break;
    case PAXOS_ACCEPTOR_STATE:
        pack_paxos_acceptor_state(p, &v->u.state);
        break;
    case PAXOS_CLIENT_VALUE:
        pack_paxos_client_value(p, &v->u.client_value);
        break;
    default:
        break;
    }
}

void unpack_paxos_message(paxos_message* v, char* p)
{
    uint16_t *raw_bytes = (uint16_t *)p;
    v->type = ntohs(*raw_bytes);
    switch (v->type) {
    case PAXOS_PREPARE:
        unpack_paxos_prepare(&v->u.prepare, p);
        break;
    case PAXOS_PROMISE:
        unpack_paxos_promise(&v->u.promise, p);
        break;
    case PAXOS_ACCEPT:
        unpack_paxos_accept(&v->u.accept, p);
        break;
    case PAXOS_ACCEPTED:
        unpack_paxos_accepted(&v->u.accepted, p);
        break;
    case PAXOS_PREEMPTED:
        unpack_paxos_preempted(&v->u.preempted, p);
        break;
    case PAXOS_REPEAT:
        unpack_paxos_repeat(&v->u.repeat, p);
        break;
    case PAXOS_TRIM:
        unpack_paxos_trim(&v->u.trim, p);
        break;
    case PAXOS_ACCEPTOR_STATE:
        unpack_paxos_acceptor_state(&v->u.state, p);
        break;
    case PAXOS_CLIENT_VALUE:
        unpack_paxos_client_value(&v->u.client_value, p);
        break;
    default:
        break;
    }
}
