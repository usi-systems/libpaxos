/* Copyright (c) 2013-2014, University of Lugano
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


#include "learner.h"
#include "khash.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct instance
{
	iid_t iid;
	uint16_t thread_id;
	ballot_t last_update_ballot;
	paxos_accepted** acks;
	paxos_accepted* final_value;
};
KHASH_MAP_INIT_INT(instance, struct instance*);

struct gap
{
	iid_t iid;
	ballot_t ballot;
	uint16_t thread_id;
	uint16_t a_tid;
	ballot_t highest_accepted_ballot;
	paxos_value* highest_accepted_value;
	uint8_t acceptor_bitmap;
	int majority;
};
KHASH_MAP_INIT_INT(gap, struct gap*);


struct learner
{
	int acceptors;
	int late_start;
	iid_t current_iid;
	iid_t highest_iid_closed;
	khash_t(instance)* instances;
	khash_t(gap)* gaps; /* Learner starts Paxos for missing instances */
};



static struct instance* learner_get_instance(struct learner* l, iid_t iid);
static struct instance* learner_get_current_instance(struct learner* l);

static struct instance* learner_get_instance_or_create(struct learner* l, iid_t iid);
static void learner_delete_instance(struct learner* l, struct instance* inst);
static struct instance* instance_new(int acceptors);
static void instance_free(struct instance* i, int acceptors);

static void instance_update(struct instance* i, paxos_accepted* ack, int acceptors, uint16_t thread_id);

static int instance_has_quorum(struct instance* i, int acceptors, uint16_t thread_id);
static void instance_add_accept(struct instance* i, paxos_accepted* ack);
static paxos_accepted* paxos_accepted_dup(paxos_accepted* ack);
static void paxos_value_copy(paxos_value* dst, paxos_value* src);
/* Extend learner to run phase 1 and phase 2 in recovery */
static struct gap* gap_new(iid_t iid, int acceptors, uint16_t l_tid);
static struct gap* get_gap_or_create(struct learner* l, iid_t iid, uint16_t l_tid);
static struct gap* learner_new_gap(struct learner* l, iid_t iid, uint16_t l_tid);
static struct gap* learner_get_gap(struct learner* l, iid_t iid);
static void gap_free(struct gap* gap);
static void gap_reset(struct gap* gap);
static int update_gap(struct gap* gap, paxos_promise* promise);
static int is_duplicated(uint8_t *acceptor_bitmap, int acceptor_id);
static void update_accepted_value(struct gap* gap, paxos_promise* promise);
static int is_majority_promised(struct gap* gap);


struct learner*
learner_new(int acceptors)
{
	struct learner* l;
	l = malloc(sizeof(struct learner));
	l->acceptors = acceptors;
	l->current_iid = 1;
	l->highest_iid_closed = 1;
	l->late_start = !paxos_config.learner_catch_up;
	l->instances = kh_init(instance);
	l->gaps = kh_init(gap);
	return l;
}

void
learner_free(struct learner* l)
{
	struct instance* inst;
	kh_foreach_value(l->instances, inst, instance_free(inst, l->acceptors));
	kh_destroy(instance, l->instances);

	struct gap* gap;
	kh_foreach_value(l->gaps, gap, gap_free(gap));
	kh_destroy(gap, l->gaps);

	free(l);
}


void
learner_set_instance_id(struct learner* l, iid_t iid)
{
	l->current_iid = iid + 1;
	l->highest_iid_closed = iid;
}


void
learner_receive_accepted(struct learner* l, paxos_accepted* ack, uint16_t thread_id)
{	
	//paxos_log_debug("---learner_receive_accepted---");
	if (l->late_start) {
		l->late_start = 0;
		l->current_iid = ack->iid;
	}
	
	if (ack->iid < l->current_iid) {
		paxos_log_debug("Dropped paxos_accepted for iid %u thread id %u. Already delivered.",
			ack->iid, thread_id);
		return;
	}
	
	struct instance* inst;
	inst = learner_get_instance_or_create(l, ack->iid);
	
	instance_update(inst, ack, l->acceptors, thread_id);
	//paxos_log_debug("1.check quorum");
	if (instance_has_quorum(inst, l->acceptors, thread_id)
		&& (inst->iid > l->highest_iid_closed))
		l->highest_iid_closed = inst->iid;
}

int learner_receive_preempted(struct learner* l, paxos_preempted* ack,
    paxos_prepare* out)
{
	struct gap* gap = learner_get_gap(l, ack->iid);
	if (gap == NULL)
		return 0;
	gap->ballot = ack->ballot;
	gap_reset(gap); // set thread_id temporary
	*out = (paxos_prepare) {gap->iid, gap->ballot, gap->thread_id};
	return 1;
}

int
learner_deliver_next(struct learner* l, paxos_accepted* out, uint16_t thread_id)
{
	struct instance* inst = learner_get_current_instance(l);
	//paxos_log_debug("---learner_delivers___");
	if (inst == NULL || !instance_has_quorum(inst, l->acceptors, thread_id))
		return 0;
	paxos_log_debug("finale value %s iid %u thread_id %u", 
		inst->final_value->value.paxos_value_val, inst->final_value->iid, inst->final_value->thread_id);
	memcpy(out, inst->final_value, sizeof(paxos_accepted));
	paxos_value_copy(&out->value, &inst->final_value->value);
	learner_delete_instance(l, inst);
	l->current_iid++;
	return 1;
}

int
learner_has_holes(struct learner* l, iid_t* from, iid_t* to)
{
	if (l->highest_iid_closed > l->current_iid) {
		*from = l->current_iid;
		*to = l->highest_iid_closed;
		return 1;
	}
	return 0;
}

void
learner_prepare_hole (struct learner* l, paxos_prepare_hole* out, iid_t* from_inst, iid_t* to_inst, uint16_t l_tid, int *msg_size)
{

	int iid;
	uint16_t max_ballot = 0;
	struct gap* gap;
	int no_iid =  *to_inst - *from_inst + 1;
	uint32_t arr [no_iid];
	out->hole.paxos_hole_len = no_iid;
	out->hole.paxos_hole_iid = (uint32_t *) malloc(sizeof(uint32_t) * no_iid);
	for (iid = *from_inst; iid <= *to_inst; iid++)
	{	 
		int idx = iid - *from_inst;
		gap = get_gap_or_create(l, iid, l_tid);
		assert(gap != NULL);
		paxos_log_debug("(*)gap->thread_id %u of thread %u gap->a_tid %u gap->iid %u", gap->thread_id, l_tid, gap->a_tid, gap->iid);
		if (gap->ballot >= max_ballot)
			max_ballot = gap->ballot;
		arr[idx] = gap->iid;
		paxos_log_debug("gap->ballot %u iid %u", gap->ballot, gap->iid);
	}
	out->iid = gap->iid;
	out->ballot = max_ballot;	
	out->thread_id = gap->thread_id;
	out->a_tid = gap->a_tid;
	out->value.paxos_value_len = 0;
	out->value.paxos_value_val = NULL;
	memcpy(out->hole.paxos_hole_iid, arr, sizeof(uint32_t) * no_iid);
	*msg_size = sizeof(paxos_message)+ (no_iid * sizeof(uint32_t));
	//paxos_log_debug("size paxos %d size of uint32 %d", sizeof(paxos_message), sizeof(uint32_t)) ;
	//paxos_log_debug("msg_size %d", *msg_size);
}

void
learner_prepare(struct learner* l, paxos_prepare* out, iid_t iid, uint16_t l_tid)
{
	//paxos_log_debug("**before is thread_id %u", l_tid);
	struct gap* gap = get_gap_or_create(l, iid, l_tid);
	assert(gap != NULL);
	paxos_log_debug("(*)gap->thread_id %u of thread %u gap->a_tid %u", gap->thread_id, l_tid, gap->a_tid);
	*out = (paxos_prepare) {gap->iid, gap->ballot, gap->thread_id, gap->a_tid};
}

/* Check if it is waiting for promises for a gap instance */
int
learner_receive_promise(struct learner* l, paxos_promise* promise,
	paxos_accept* accept)
{
	paxos_log_debug("promise for iid %u ballot %u t_id %u a_tid %u value_ballot %u acceptor id %u, value %s",
		promise->iid,
		promise->ballot,
		promise->thread_id,
		promise->a_tid,
		promise->value_ballot,
		promise->aid,
		promise->value.paxos_value_val);
	struct gap* gap = learner_get_gap(l, promise->iid);
	if (gap == NULL)
		return 0;
	int reached_majority = update_gap(gap, promise);
	if (!reached_majority)
		return 0;
	accept->iid = gap->iid;
	accept->ballot = gap->ballot;
	accept->thread_id = gap->thread_id;
	accept->a_tid = gap->a_tid;
	//paxos_log_debug("aid -- gap->a_tid %u accept->a_tid %u", gap->a_tid, accept->a_tid);
	accept->aid = promise->aid;
	if (gap->highest_accepted_value)
	{
		paxos_value_copy(&accept->value, gap->highest_accepted_value);
		assert(accept->value.paxos_value_len ==
			gap->highest_accepted_value->paxos_value_len);
	} else {
		accept->value.paxos_value_len = 0;
		accept->value.paxos_value_val = NULL;
	}
	paxos_log_debug("send accept_msg to acceptor, iid %u ballot %u t_id %u a_tid %u value_ballot %u aid %u, value %s",
		accept->iid,
		accept->ballot,
		accept->thread_id,
		accept->a_tid,
		accept->value_ballot,
		accept->aid,
		accept->value.paxos_value_val);
	return 1;
}

static int
update_gap(struct gap* gap, paxos_promise* promise)
{
	// Check if this promise has been processed (same acceptor id)
	if (is_duplicated(&gap->acceptor_bitmap, promise->aid))
		return 0;
	//update thread_id and a_tid
	gap->thread_id = promise->thread_id;
	gap->a_tid = promise->a_tid;
	// update highest value
	update_accepted_value(gap, promise);
	// Reach quorum?
	return is_majority_promised(gap);
}

static int
is_majority_promised(struct gap* gap)
{
	return __builtin_popcount(gap->acceptor_bitmap) >= gap->majority;
}

static void
update_accepted_value(struct gap* gap, paxos_promise* promise)
{
	//paxos_log_debug("--Gap highest_accepted_ballot %u Promise value_ballot %u", 
	//					gap->highest_accepted_ballot, promise->value_ballot);

	if (gap->highest_accepted_ballot <= promise->value_ballot)
	{
		gap->highest_accepted_ballot = promise->value_ballot;
		//paxos_log_debug("--Promise paxos_value_len %d", promise->value.paxos_value_len);
		if (promise->value.paxos_value_len)
		{
			if (gap->highest_accepted_value == NULL)
				gap->highest_accepted_value = malloc(sizeof(paxos_value*));
			paxos_value_copy(gap->highest_accepted_value, &promise->value);
			//paxos_log_debug("--Gap paxos_value_len %d",gap->highest_accepted_value->paxos_value_len);
		}
	}
}

/* Check if the acceptor id has been included */
static int
is_duplicated(uint8_t *acceptor_bitmap, int acceptor_id)
{
	uint8_t mask = 1 << acceptor_id;
	/* if true, the acceptor_id existed */
	if (*acceptor_bitmap & mask)
		return 1;
	/* else, include the acceptor id */
	*acceptor_bitmap |= mask;
	return 0;
}

static struct gap*
get_gap_or_create(struct learner* l, iid_t iid, uint16_t l_tid)
{
	//paxos_log_debug("**get_gap_or_create has thread_id %u", l_tid);
	struct gap* gap = learner_get_gap(l, iid);
	if (gap == NULL) {
		gap = learner_new_gap(l, iid, l_tid);
	} else {
		gap_reset(gap);
	}
	return gap;
}

static struct gap*
learner_get_gap(struct learner* l, iid_t iid)
{
	khiter_t k;
	k = kh_get_gap(l->gaps, iid);
	if (k == kh_end(l->gaps))
		return NULL;
	return kh_value(l->gaps, k);
}

static struct gap*
learner_new_gap(struct learner* l, iid_t iid, uint16_t l_tid)
{
	//paxos_log_debug("**learner_new_gap has thread_id %u", l_tid);
	int absent;
	khiter_t k = kh_put_gap(l->gaps, iid, &absent);
	assert(absent);
	struct gap* gap = gap_new(iid, l->acceptors, l_tid);
	kh_value(l->gaps, k) = gap;
	return gap;
}

/* Initialize an prepare instance with a starting ballot of 3. */
static struct gap*
gap_new(iid_t iid, int acceptors, uint16_t l_tid)
{
	struct gap* gap;
	gap = malloc(sizeof(struct gap));
	memset(gap, 0, sizeof(struct gap));
	gap->iid = iid;
	gap->ballot = 3;
	gap->thread_id = l_tid;
	gap->a_tid = l_tid; //NUM_OF_THREAD
	gap->highest_accepted_value = NULL;
	gap->majority = paxos_quorum(acceptors);
	paxos_log_debug("**gap_new has iid %u ballot %u thread_id %u", gap->iid, gap->ballot, l_tid);
	return gap;
}

/* Reset an prepare instance and increase the ballot by 11. */
static void
gap_reset(struct gap* gap)
{
	
	gap->ballot += 11; // MAXIMUM 11 Learner
	//gap->thread_id = l_tid; // arbitrary, except threads < 8 or 12
	if (gap->highest_accepted_value)
		paxos_value_free(gap->highest_accepted_value);
	gap->highest_accepted_value = NULL;
	gap->highest_accepted_ballot = 0;
	gap->acceptor_bitmap = 0;
	paxos_log_debug("**gap reset has iid %u ballot %u", gap->iid, gap->ballot);
}

static void
gap_free(struct gap* gap)
{
	if (gap->highest_accepted_value != NULL)
		paxos_value_free(gap->highest_accepted_value);
	free(gap);
}

static struct instance*
learner_get_instance(struct learner* l, iid_t iid)
{
	khiter_t k;
	k = kh_get_instance(l->instances, iid);
	if (k == kh_end(l->instances))
		return NULL;
	return kh_value(l->instances, k);
}


static struct instance*
learner_get_current_instance(struct learner* l)
{
	return learner_get_instance(l, l->current_iid);
	return NULL;
}


static struct instance*
learner_get_instance_or_create(struct learner* l, iid_t iid)
{
	struct instance* inst = learner_get_instance(l, iid);
	if (inst == NULL) {
		int rv;
		khiter_t k = kh_put_instance(l->instances, iid, &rv);
		assert(rv != -1);
		inst = instance_new(l->acceptors);
		kh_value(l->instances, k) = inst;
	}
	return inst;
}

static void
learner_delete_instance(struct learner* l, struct instance* inst)
{
	khiter_t k;
	k = kh_get_instance(l->instances, inst->iid);
	kh_del_instance(l->instances, k);
	instance_free(inst, l->acceptors);
}

static struct instance*
instance_new(int acceptors)
{
	int i;
	struct instance* inst;
	inst = malloc(sizeof(struct instance));
	memset(inst, 0, sizeof(struct instance));
	inst->acks = malloc(sizeof(paxos_accepted*) * acceptors);
	for (i = 0; i < acceptors; ++i)
		inst->acks[i] = NULL;
	return inst;
}

static void
instance_free(struct instance* inst, int acceptors)
{
	int i;
	for (i = 0; i < acceptors; i++)
		if (inst->acks[i] != NULL)
			paxos_accepted_free(inst->acks[i]);
	free(inst->acks);
	free(inst);
}

static void
instance_update(struct instance* inst, paxos_accepted* accepted, int acceptors, uint16_t thread_id)
{	
	if (inst->iid == 0) {
		paxos_log_debug("Received first message for iid: %u thread id %u aid %u", accepted->iid, thread_id,accepted->aid);
		inst->iid = accepted->iid;
		inst->last_update_ballot = accepted->ballot;
	}
	
	if (instance_has_quorum(inst, acceptors, thread_id)) {
		paxos_log_debug("Dropped paxos_accepted iid %u thread id %u. Already closed.",
			accepted->iid, thread_id);
		return;
	}
	
	if (accepted->aid > acceptors - 1) {
		paxos_log_debug("Invalid acceptor id: %d thread id %u", accepted->aid, thread_id);
		return;
	}
	paxos_accepted* prev_accepted = inst->acks[accepted->aid];

	/*paxos_log_debug("accepted->aid %u "	"with accepted->iid %u thread_id %u accepted->len %d accepted->value %s",
						accepted->aid, 
						accepted->iid, 
						thread_id, 
						accepted->value.paxos_value_len, 
						accepted->value.paxos_value_val);*/

	if (prev_accepted != NULL && prev_accepted->ballot >= accepted->ballot) {
		//paxos_log_debug("prev_accepted->ballot %u accepted->ballot %u\n",
		//				prev_accepted->ballot, accepted->ballot);
		paxos_log_debug("Dropped paxos_accepted for iid %u thread id %u."
			"Previous ballot is newer or equal.", accepted->iid, thread_id);
		return;
	}
	
	instance_add_accept(inst, accepted);
}

/* 
	Checks if a given instance is closed, that is if a quorum of acceptor 
	accepted the same value ballot pair. 
	Returns 1 if the instance is closed, 0 otherwise.
*/
static int 
instance_has_quorum(struct instance* inst, int acceptors, uint16_t thread_id)
{
	paxos_accepted* curr_ack;
	int i, a_valid_index = -1, count = 0;

	if (inst->final_value != NULL)
		return 1;
	
	for (i = 0; i < acceptors; i++) {
		//paxos_log_debug("i %d",i);
		curr_ack = inst->acks[i];
	
		// Skip over missing acceptor acks
		if (curr_ack == NULL) continue;
		
		// Count the ones "agreeing" with the last added
		if (curr_ack->ballot == inst->last_update_ballot)
		{
			count++;
			a_valid_index = i;
			/*paxos_log_debug("*ack aid %d iid %u thread_id %u value %s at learner_thread_id %u",
						i, 
						inst->acks[i]->iid, 
						inst->acks[i]->thread_id,
						inst->acks[i]->value.paxos_value_val,
						thread_id);*/
		}
	}

	if (count >= paxos_quorum(acceptors)) {
		inst->final_value = inst->acks[a_valid_index];
		paxos_log_debug("*Reached quorum, iid: %u thread id %u at aid %d iid %u value %s is closed!", 
			inst->iid, 
			thread_id,
			inst->final_value->aid,
			inst->final_value->iid,
			inst->final_value->value.paxos_value_val);
		return 1;
	}
	return 0;
}

/*
	Adds the given paxos_accepted to the given instance, 
	replacing the previous paxos_accepted, if any.
*/
static void
instance_add_accept(struct instance* inst, paxos_accepted* accepted)
{
	int acceptor_id = accepted->aid;
	if (inst->acks[acceptor_id] != NULL)
		paxos_accepted_free(inst->acks[acceptor_id]);
	inst->acks[acceptor_id] = paxos_accepted_dup(accepted);
	inst->last_update_ballot = accepted->ballot;
}

/*s
	Returns a copy of it's argument.
*/
static paxos_accepted*
paxos_accepted_dup(paxos_accepted* ack)
{
	paxos_accepted* copy;
	copy = malloc(sizeof(paxos_accepted));
	memcpy(copy, ack, sizeof(paxos_accepted));
	paxos_value_copy(&copy->value, &ack->value);
	return copy;
}

/*static void
paxos_hole_iid_copy(paxos_hole* dst, paxos_hole* src)
{
	
}*/

static void
paxos_value_copy(paxos_value* dst, paxos_value* src)
{
	int len = src->paxos_value_len;
	dst->paxos_value_len = len;
	if (src->paxos_value_val != NULL) {
		dst->paxos_value_val = malloc(len);
		memcpy(dst->paxos_value_val, src->paxos_value_val, len);	
	}
}
