#ifndef MESSAGE_PACK_H_
#define MESSAGE_PACK_H_

#include "paxos_types.h"

void pack_paxos_accept(char* p, paxos_accept* v);
void unpack_paxos_accept(paxos_accept* v, char* p);
void pack_paxos_message(char* p, paxos_message* v);
void unpack_paxos_message(paxos_message* v, char* p);

#endif