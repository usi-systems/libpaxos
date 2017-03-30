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
#define THREAD_ID_OFFSET 8
#define ACCEPTOR_COUNTER_ID 10
#define VALUE_BALLOT_OFFSET 12
#define AID_OFFSET 14
#define VALUE_OFFSET 16

/* paxos_repeat offsets */
#define FROM_OFFSET 2
#define TO_OFFSET 6
#define REPEAT_THREAD_ID_OFFSET 10
#define REPEAT_ACCEPTOR_COUNTER_ID 12
/* paxos_trim offset */
#define TRIM_OFFSET 2
/* paxos_acceptor_state offsets */
#define STATE_AID_OFFSET 2
#define TRIM_IID_OFFSET 6


void pack_uint32(char *p, uint32_t value, int offset)
{
    uint32_t *serialize = (uint32_t *)(p + offset);
    *serialize = htonl(value);
    //printf("pack_uint32 %u\n", *serialize);
}

void pack_uint16(char *p, uint16_t value, int offset)
{
    uint16_t *serialize = (uint16_t *)(p + offset);
    *serialize = htons(value);
}

void pack_value(char *p, paxos_value *v, int offset)
{
    //printf("paxos_value_len %d offset %d\n",v->paxos_value_len, offset);
    pack_uint32(p, v->paxos_value_len, offset);
    char *value =  (char *)(p + offset + 4);
    memcpy(value, v->paxos_value_val, v->paxos_value_len);
}
void pack_hole_value(char *p, paxos_hole *v, int offset)
{
    //printf("paxos_hole_len %d offset %d\n",v->paxos_hole_len, offset);
    pack_uint32(p, v->paxos_hole_len, offset);
    uint32_t *hole_value =  (uint32_t *)(p + offset + 4);
    memcpy(hole_value, v->paxos_hole_iid, v->paxos_hole_len * sizeof(uint32_t));
}

void unpack_uint32(uint32_t *out_value, char *p, int offset)
{
    uint32_t *raw_bytes = (uint32_t *)(p + offset);
    *out_value = ntohl(*raw_bytes);
   //printf("unpack_uint32 %u\n", *out_value);
}

void unpack_uint16(uint16_t *out_value, char *p, int offset)
{
    uint16_t *raw_bytes = (uint16_t *)(p + offset);
    *out_value = ntohs(*raw_bytes);
}

void unpack_value(paxos_value *v, char *p, int offset)
{
    //printf("unpaxos_value_len %d offset %d\n",v->paxos_value_len, offset);
    unpack_uint32((uint32_t *)&v->paxos_value_len, p, offset);
    char *value =  (char *)(p + offset + 4);
    if (v->paxos_value_len > 0) {
        v->paxos_value_val = malloc(v->paxos_value_len + 1);
        memcpy(v->paxos_value_val, value, v->paxos_value_len);
        v->paxos_value_val[v->paxos_value_len] = '\0';
    } else {
        v->paxos_value_val = NULL;
    }
}
void unpack_hole_value(paxos_hole *v, char *p, int offset)
{
    //printf("unpaxos_hole_len %d offset %d\n",v->paxos_hole_len, offset);
    unpack_uint32((uint32_t *)&v->paxos_hole_len, p, offset);
    uint32_t *value =  (uint32_t *)(p + offset + 4);
    if (v->paxos_hole_len > 0) {
        v->paxos_hole_iid = malloc(v->paxos_hole_len + 1);
        memcpy(v->paxos_hole_iid, value, v->paxos_hole_len * sizeof(uint32_t));
    } else {
        v->paxos_hole_iid = NULL;
    }
}

void pack_paxos_prepare(char* p, paxos_prepare* v)
{
    pack_uint16(p, PAXOS_PREPARE, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->thread_id, THREAD_ID_OFFSET);
    pack_uint16(p, v->a_tid, ACCEPTOR_COUNTER_ID);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
}
void pack_paxos_prepare_hole (char* p, paxos_prepare_hole* v)
{
    //printf("pack\n");
    pack_uint16(p, PAXOS_PREPARE_HOLE, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->thread_id, THREAD_ID_OFFSET);
    pack_uint16(p, v->a_tid, ACCEPTOR_COUNTER_ID);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
    //printf("sizeof char * %lu\n", sizeof(char *));
    pack_hole_value(p, &v->hole, (VALUE_OFFSET + 4 + sizeof(char *)));
}
void unpack_paxos_prepare(paxos_prepare* v, char* p)
{
    unpack_uint32(&v->iid, p, IID_OFFSET);
    unpack_uint16(&v->ballot, p, BALLOT_OFFSET);
    unpack_uint16(&v->thread_id, p, THREAD_ID_OFFSET);
    unpack_uint16(&v->a_tid, p, ACCEPTOR_COUNTER_ID);
    unpack_uint16(&v->value_ballot, p, VALUE_BALLOT_OFFSET);
    unpack_uint16(&v->aid, p, AID_OFFSET);
    unpack_value(&v->value, p, VALUE_OFFSET);
}
void unpack_paxos_prepare_hole(paxos_prepare_hole* v, char* p)
{
    unpack_uint32(&v->iid, p, IID_OFFSET);
    unpack_uint16(&v->ballot, p, BALLOT_OFFSET);
    unpack_uint16(&v->thread_id, p, THREAD_ID_OFFSET);
    unpack_uint16(&v->a_tid, p, ACCEPTOR_COUNTER_ID);
    unpack_uint16(&v->value_ballot, p, VALUE_BALLOT_OFFSET);
    unpack_uint16(&v->aid, p, AID_OFFSET);
    unpack_value(&v->value, p, VALUE_OFFSET);
    unpack_hole_value(&v->hole, p, (VALUE_OFFSET + 4 + sizeof(char *)));
}

void pack_paxos_promise(char* p, paxos_promise* v)
{
    pack_uint16(p, PAXOS_PROMISE, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->thread_id, THREAD_ID_OFFSET);
    pack_uint16(p, v->a_tid, ACCEPTOR_COUNTER_ID);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
}
void pack_paxos_promise_hole(char* p, paxos_promise* v)
{
    pack_uint16(p, PAXOS_PROMISE_HOLE, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->thread_id, THREAD_ID_OFFSET);
    pack_uint16(p, v->a_tid, ACCEPTOR_COUNTER_ID);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
}
void unpack_paxos_promise(paxos_promise* v, char* p)
{
    unpack_uint32(&v->iid, p, IID_OFFSET);
    unpack_uint16(&v->ballot, p, BALLOT_OFFSET);
    unpack_uint16(&v->thread_id, p, THREAD_ID_OFFSET);
    unpack_uint16(&v->a_tid, p, ACCEPTOR_COUNTER_ID);
    unpack_uint16(&v->value_ballot, p, VALUE_BALLOT_OFFSET);
    unpack_uint16(&v->aid, p, AID_OFFSET);
    unpack_value(&v->value, p, VALUE_OFFSET);
}


void pack_paxos_accept(char* p, paxos_accept* v)
{
    pack_uint16(p, PAXOS_ACCEPT, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->thread_id, THREAD_ID_OFFSET);
    pack_uint16(p, v->a_tid, ACCEPTOR_COUNTER_ID);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
}
void pack_paxos_accept_hole(char* p, paxos_accept* v)
{
    pack_uint16(p, PAXOS_ACCEPT_HOLE, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->thread_id, THREAD_ID_OFFSET);
    pack_uint16(p, v->a_tid, ACCEPTOR_COUNTER_ID);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
}
void unpack_paxos_accept(paxos_accept* v, char* p)
{
    unpack_uint32(&v->iid, p, IID_OFFSET);
    unpack_uint16(&v->ballot, p, BALLOT_OFFSET);
    unpack_uint16(&v->thread_id, p, THREAD_ID_OFFSET);
    unpack_uint16(&v->a_tid, p, ACCEPTOR_COUNTER_ID);
    unpack_uint16(&v->value_ballot, p, VALUE_BALLOT_OFFSET);
    unpack_uint16(&v->aid, p, AID_OFFSET);
    unpack_value(&v->value, p, VALUE_OFFSET);
}

void pack_paxos_accepted(char* p, paxos_accepted* v)
{
    pack_uint16(p, PAXOS_ACCEPTED, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->thread_id, THREAD_ID_OFFSET);
    pack_uint16(p, v->a_tid, ACCEPTOR_COUNTER_ID);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
}

void unpack_paxos_accepted(paxos_accepted* v, char* p)
{
    unpack_uint32(&v->iid, p, IID_OFFSET);
    unpack_uint16(&v->ballot, p, BALLOT_OFFSET);
    unpack_uint16(&v->thread_id, p, THREAD_ID_OFFSET);
    unpack_uint16(&v->a_tid, p, ACCEPTOR_COUNTER_ID);
    unpack_uint16(&v->value_ballot, p, VALUE_BALLOT_OFFSET);
    unpack_uint16(&v->aid, p, AID_OFFSET);
    unpack_value(&v->value, p, VALUE_OFFSET);
}

void pack_paxos_preempted(char* p, paxos_preempted* v)
{
    pack_uint16(p, PAXOS_PREEMPTED, MSGTYPE_OFFSET);
    pack_uint32(p, v->iid, IID_OFFSET);
    pack_uint16(p, v->ballot, BALLOT_OFFSET);
    pack_uint16(p, v->thread_id, THREAD_ID_OFFSET);
    pack_uint16(p, v->a_tid, ACCEPTOR_COUNTER_ID);
    pack_uint16(p, v->value_ballot, VALUE_BALLOT_OFFSET);
    pack_uint16(p, v->aid, AID_OFFSET);
    pack_value(p, &v->value, VALUE_OFFSET);
}

void unpack_paxos_preempted(paxos_preempted* v, char* p)
{
    unpack_uint32(&v->iid, p, IID_OFFSET);
    unpack_uint16(&v->ballot, p, BALLOT_OFFSET);
    unpack_uint16(&v->thread_id, p, THREAD_ID_OFFSET);
    unpack_uint16(&v->a_tid, p, ACCEPTOR_COUNTER_ID);
    unpack_uint16(&v->value_ballot, p, VALUE_BALLOT_OFFSET);
    unpack_uint16(&v->aid, p, AID_OFFSET);
    unpack_value(&v->value, p, VALUE_OFFSET);
}

void pack_paxos_repeat(char* p, paxos_repeat* v)
{
    pack_uint16(p, PAXOS_REPEAT, MSGTYPE_OFFSET);
    pack_uint32(p, v->from, FROM_OFFSET);
    pack_uint32(p, v->to, TO_OFFSET);
    pack_uint16(p, v->thread_id, REPEAT_THREAD_ID_OFFSET);
    pack_uint16(p, v->a_tid, REPEAT_ACCEPTOR_COUNTER_ID);
}

void unpack_paxos_repeat(paxos_repeat* v, char* p)
{
    unpack_uint32(&v->from, p, FROM_OFFSET);
    unpack_uint32(&v->to, p, TO_OFFSET);
    unpack_uint16(&v->thread_id, p, REPEAT_THREAD_ID_OFFSET);
    unpack_uint16(&v->a_tid, p, REPEAT_ACCEPTOR_COUNTER_ID);
    
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

size_t pack_paxos_message(char* p, paxos_message* v)
{
    size_t msglen = 0;
    switch (v->type) {
    case PAXOS_PREPARE:
        pack_paxos_prepare(p, &v->u.prepare);
        msglen = sizeof(paxos_message);
        break;
    case PAXOS_PROMISE:
        pack_paxos_promise(p, &v->u.promise);
        msglen = sizeof(paxos_message) + v->u.promise.value.paxos_value_len;
        break;
    case PAXOS_ACCEPT:
        pack_paxos_accept(p, &v->u.accept);
        msglen = sizeof(paxos_message) + v->u.accept.value.paxos_value_len;
        break;
    case PAXOS_ACCEPTED:
        pack_paxos_accepted(p, &v->u.accepted);
        msglen = sizeof(paxos_message) + v->u.accepted.value.paxos_value_len;
        break;
    case PAXOS_PREEMPTED:
        pack_paxos_preempted(p, &v->u.preempted);
        msglen = sizeof(paxos_message);
        break;
    case PAXOS_REPEAT:
        pack_paxos_repeat(p, &v->u.repeat);
        msglen = sizeof(paxos_message);
        break;
    case PAXOS_TRIM:
        pack_paxos_trim(p, &v->u.trim);
        msglen = sizeof(paxos_message);
        break;
    case PAXOS_ACCEPTOR_STATE:
        pack_paxos_acceptor_state(p, &v->u.state);
        msglen = sizeof(paxos_message);
        break;
    case PAXOS_CLIENT_VALUE:
        pack_paxos_client_value(p, &v->u.client_value);
        msglen = sizeof(paxos_message);
        break;
    case PAXOS_PREPARE_HOLE:
        pack_paxos_prepare_hole(p, &v->u.prepare_hole);
        msglen = sizeof(paxos_message) + v->u.prepare_hole.hole.paxos_hole_len;
        break;
     case PAXOS_PROMISE_HOLE:
        pack_paxos_promise_hole(p, &v->u.promise);
        msglen = sizeof(paxos_message) + v->u.promise.value.paxos_value_len;
        break;
    case PAXOS_ACCEPT_HOLE:
        pack_paxos_accept_hole(p, &v->u.accept);
        msglen = sizeof(paxos_message) + v->u.accept.value.paxos_value_len;
        break;
    default:
        break;
    }
    return msglen;
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
    case PAXOS_PREPARE_HOLE:
        unpack_paxos_prepare_hole(&v->u.prepare_hole, p);
        break;
     case PAXOS_PROMISE_HOLE:
        unpack_paxos_promise(&v->u.promise, p);
        break;
    case PAXOS_ACCEPT_HOLE:
        unpack_paxos_accept(&v->u.accept, p);
        break;
    default:
        break;
    }
}
