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


#include "learner.h"
#include "khash.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct instance
{
	iid_t iid;
	ballot_t last_update_ballot;
	paxos_accepted** acks;
	paxos_accepted* final_value;
};
KHASH_MAP_INIT_INT(instance, struct instance*);

struct gap
{
	iid_t iid;
	ballot_t ballot;
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
	khash_t(gap)* gaps;	/* Learner starts Paxos for missing instances */
};

static struct instance* learner_get_instance(struct learner* l, iid_t iid);
static struct instance* learner_get_current_instance(struct learner* l);
static struct instance* learner_get_instance_or_create(struct learner* l, 
	iid_t iid);
static void learner_delete_instance(struct learner* l, struct instance* inst);
static struct instance* instance_new(int acceptors);
static void instance_free(struct instance* i, int acceptors);
static void instance_update(struct instance* i, paxos_accepted* ack, int acceptors);
static int instance_has_quorum(struct instance* i, int acceptors);
static void instance_add_accept(struct instance* i, paxos_accepted* ack);
static paxos_accepted* paxos_accepted_dup(paxos_accepted* ack);
static void paxos_value_copy(paxos_value* dst, paxos_value* src);
/* Extend learner to run phase 1 and phase 2 in recovery */
static struct gap* gap_new(iid_t iid, int acceptors);
static struct gap* get_gap_or_create(struct learner* l, iid_t iid);
static struct gap* learner_new_gap(struct learner* l, iid_t iid);
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
learner_receive_accepted(struct learner* l, paxos_accepted* ack)
{	
	if (l->late_start) {
		l->late_start = 0;
		l->current_iid = ack->iid;
	}
	
	if (ack->iid < l->current_iid) {
		paxos_log_debug("Dropped paxos_accepted for iid %u. Already delivered.",
			ack->iid);
		return;
	}
	
	struct instance* inst;
	inst = learner_get_instance_or_create(l, ack->iid);
	
	instance_update(inst, ack, l->acceptors);
	
	if (instance_has_quorum(inst, l->acceptors)
		&& (inst->iid > l->highest_iid_closed))
		l->highest_iid_closed = inst->iid;
}

int
learner_deliver_next(struct learner* l, paxos_accepted* out)
{
	struct instance* inst = learner_get_current_instance(l);
	if (inst == NULL || !instance_has_quorum(inst, l->acceptors))
		return 0;
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
learner_prepare(struct learner* l, paxos_prepare* out, iid_t iid)
{
	struct gap* gap = get_gap_or_create(l, iid);
	assert(gap != NULL);
	*out = (paxos_prepare) {gap->iid, gap->ballot};
}

/* Check if it is waiting for promises for a gap instance */
int
learner_receive_promise(struct learner* l, paxos_promise* promise,
	paxos_accept* accept)
{
	paxos_log_debug("promise for iid %u.", promise->iid);

	struct gap* gap = learner_get_gap(l, promise->iid);
	if (gap == NULL)
		return 0;
	int reached_majority = update_gap(gap, promise);
	if (!reached_majority)
		return 0;
	accept->iid = gap->iid;
	accept->ballot = gap->ballot;
	if (gap->highest_accepted_value) {
		paxos_value_copy(&accept->value, gap->highest_accepted_value);
		assert(accept->value.paxos_value_len ==
			gap->highest_accepted_value->paxos_value_len);
	} else {
		accept->value.paxos_value_len = 0;
		accept->value.paxos_value_val = NULL;
	}
	return 1;
}

static int
update_gap(struct gap* gap, paxos_promise* promise)
{
	// Check if this promise has been processed (same acceptor id)
	if (is_duplicated(&gap->acceptor_bitmap, promise->aid))
		return 0;
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
	if (gap->highest_accepted_ballot < promise->value_ballot) {
		gap->highest_accepted_ballot = promise->value_ballot;
		if (promise->value.paxos_value_len) {
			if (gap->highest_accepted_value == NULL)
				gap->highest_accepted_value = malloc(sizeof(paxos_value*));
			paxos_value_copy(gap->highest_accepted_value, &promise->value);
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
get_gap_or_create(struct learner* l, iid_t iid)
{
	struct gap* gap = learner_get_gap(l, iid);
	if (gap == NULL) {
		gap = learner_new_gap(l, iid);
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
learner_new_gap(struct learner* l, iid_t iid)
{
	int absent;
	khiter_t k = kh_put_gap(l->gaps, iid, &absent);
	assert(absent);
	struct gap* gap = gap_new(iid, l->acceptors);
	kh_value(l->gaps, k) = gap;
	return gap;
}

/* Initialize an prepare instance with a starting ballot of 3. */
static struct gap*
gap_new(iid_t iid, int acceptors)
{
	struct gap* gap;
	gap = malloc(sizeof(struct gap));
	memset(gap, 0, sizeof(struct gap));
	gap->iid = iid;
	gap->ballot = 3;
	gap->highest_accepted_value = NULL;
	gap->majority = paxos_quorum(acceptors);
	return gap;
}

/* Reset an prepare instance and increase the ballot by 11. */
static void
gap_reset(struct gap* gap)
{
	gap->ballot += 11; // MAXIMUM 11 Learner
	if (gap->highest_accepted_value)
		paxos_value_free(gap->highest_accepted_value);
	gap->highest_accepted_value = NULL;
	gap->highest_accepted_ballot = 0;
	gap->acceptor_bitmap = 0;
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
instance_update(struct instance* inst, paxos_accepted* accepted, int acceptors)
{	
	if (inst->iid == 0) {
		paxos_log_debug("Received first message for iid: %u", accepted->iid);
		inst->iid = accepted->iid;
		inst->last_update_ballot = accepted->ballot;
	}
	
	if (instance_has_quorum(inst, acceptors)) {
		paxos_log_debug("Dropped paxos_accepted iid %u. Already closed.",
			accepted->iid);
		return;
	}
	
	paxos_accepted* prev_accepted = inst->acks[accepted->aid];
	if (prev_accepted != NULL && prev_accepted->ballot >= accepted->ballot) {
		paxos_log_debug("Dropped paxos_accepted for iid %u."
			"Previous ballot is newer or equal.", accepted->iid);
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
instance_has_quorum(struct instance* inst, int acceptors)
{
	paxos_accepted* curr_ack;
	int i, a_valid_index = -1, count = 0;

	if (inst->final_value != NULL)
		return 1;
	
	for (i = 0; i < acceptors; i++) {
		curr_ack = inst->acks[i];
	
		// Skip over missing acceptor acks
		if (curr_ack == NULL) continue;
		
		// Count the ones "agreeing" with the last added
		if (curr_ack->ballot == inst->last_update_ballot) {
			count++;
			a_valid_index = i;
		}
	}

	if (count >= paxos_quorum(acceptors)) {
		paxos_log_debug("Reached quorum, iid: %u is closed!", inst->iid);
		inst->final_value = inst->acks[a_valid_index];
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

/*
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
