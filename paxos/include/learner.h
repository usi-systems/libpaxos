/*
 * Copyright (c) 2013-2014, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _LEARNER_H_
#define _LEARNER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "paxos.h"

struct learner;
struct learner_thread;

struct learner* learner_new(int acceptors);
void learner_free(struct learner* l);

void learner_set_instance_id(struct learner* l, iid_t iid);
//void learner_thread_set_instance_id(struct learner_thread* l, iid_t iid);

void learner_receive_accepted(struct learner* l, paxos_accepted* ack, uint16_t thread_id);

int learner_deliver_next(struct learner* l, paxos_accepted* out, uint16_t thread_id);

int learner_has_holes(struct learner* l, iid_t* from, iid_t* to);
/* Extend learner to run phase 1 and phase 2 in recovery */
//void learner_prepare(struct learner* l, paxos_prepare* out, iid_t iid);
void learner_prepare(struct learner* l, paxos_prepare* out, iid_t iid, uint16_t lth_id);
int learner_receive_promise(struct learner* l, paxos_promise* promise, paxos_accept* accept);
int learner_receive_preempted(struct learner* l, paxos_preempted* ack,
    paxos_prepare* out);

#ifdef __cplusplus
}
#endif

#endif
