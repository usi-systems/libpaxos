#ifndef _PAXOS_H_

#include <time.h>

#define NEW_COMMAND        0x08
#define LEARNER_CHECKPOINT 0x22


struct paxos_hdr {
    uint8_t msgtype;
    uint8_t worker_id;
    uint16_t rnd;
    uint32_t inst;
    uint16_t log_index;
    uint16_t vrnd;
    uint16_t acptid;
    uint16_t reserved;
    uint64_t value;
    uint32_t reserved2;
    struct timespec igress_ts;
} __attribute__((__packed__));


void print_paxos_hdr(struct paxos_hdr* paxos);
void serialize_paxos_hdr(struct paxos_hdr* paxos);
void deserialize_paxos_hdr(struct paxos_hdr* paxos);

#endif
