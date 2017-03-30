#ifndef MESSAGE_PACK_H_
#define MESSAGE_PACK_H_

#include "paxos_types.h"
void pack_paxos_prepare(char* p, paxos_prepare* v);
void pack_paxos_prepare_hole (char* p, paxos_prepare_hole* v);
void pack_paxos_promise(char* p, paxos_promise* v);
void pack_paxos_accept(char* p, paxos_accept* v);
void pack_paxos_accepted(char* p, paxos_accepted* v);
void pack_paxos_preempted(char* p, paxos_preempted* v);
void pack_paxos_repeat(char* p, paxos_repeat* v);
void pack_paxos_trim(char* p, paxos_trim* v);
void pack_paxos_acceptor_state(char* p, paxos_acceptor_state* v);
void pack_paxos_client_value(char* p, paxos_client_value* v);
size_t pack_paxos_message(char* p, paxos_message* v);
void unpack_paxos_prepare(paxos_prepare* v, char* p);
void unpack_paxos_prepare_hole(paxos_prepare_hole* v, char* p);
void unpack_paxos_promise(paxos_promise* v, char* p);
void unpack_paxos_accept(paxos_accept* v, char* p);
void unpack_paxos_accepted(paxos_accepted* v, char* p);
void unpack_paxos_preempted(paxos_preempted* v, char* p);
void unpack_paxos_repeat(paxos_repeat* v, char* p);
void unpack_paxos_trim(paxos_trim* v, char* p);
void unpack_paxos_acceptor_state(paxos_acceptor_state* v, char* p);
void unpack_paxos_client_value(paxos_client_value* v, char* p);
void unpack_paxos_message(paxos_message* v, char* p);

#endif