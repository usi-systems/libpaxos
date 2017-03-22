/*Copyright (c) 2013-2014, University of Lugano
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


#include "acceptor.h"
#include "storage.h"
#include <stdlib.h>
#include <string.h>

struct acceptor
{
	int id;
	iid_t *trim_iid;
	struct storage* store;
};

static void paxos_accepted_to_promise(paxos_accepted* acc, paxos_message* out);
static void paxos_accept_to_accepted(int id, paxos_accept* acc, paxos_message* out);
static void paxos_accepted_to_preempted(int id, paxos_accepted* acc, paxos_message* out);


struct acceptor*
acceptor_new(int id)
{
	struct acceptor* a;
	a = malloc(sizeof(struct acceptor));
	a->id = id;
	a->store = malloc (NUM_OF_THREAD * sizeof ( struct storage));
	a->trim_iid = malloc (sizeof(iid_t));
	int i;
	for (i = 0; i < NUM_OF_THREAD; i++)
	{
		//&a->store[i] = malloc(sizeof(struct storage*));
		storage_init(&a->store[i], id);
		if (storage_open(&a->store[i]) != 0) {
			free(a);
			return NULL;
		}
		if (storage_tx_begin(&a->store[i]) != 0)
			return NULL;
		
		a->trim_iid[i] = storage_get_trim_instance(&a->store[i]);
		paxos_log_debug("--thread %u trim_iid %u", i,a->trim_iid[i]);
		if (storage_tx_commit(&a->store[i]) != 0)
			return NULL;
	}
	
	return a;
}

void
acceptor_free(struct acceptor* a) 
{
	int i;
	for (i = 0; i < NUM_OF_THREAD; i++)
	{
		storage_close(&a->store[i]);
	}
	
	free(a);
}

int
acceptor_receive_prepare(struct acceptor* a, 
	paxos_prepare* req, paxos_message* out, int t_id)
{
	paxos_log_debug("\nreceive prepare message of thread_id %d with iid %u a->trim_iid[%d] %u", t_id,req->iid, t_id, a->trim_iid[t_id]);
	paxos_accepted acc;
	if (req->iid <= a->trim_iid[t_id])
		return 0;
	memset(&acc, 0, sizeof(paxos_accepted));
	if (storage_tx_begin(&a->store[t_id]) != 0)
		return 0;
	int found = storage_get_record(&a->store[t_id], req->iid, &acc);
	paxos_log_debug("prepare found %d req->iid %u acc->ballot %u acc->value_ballot %u",
						found, req->iid, acc.ballot, acc.value_ballot);
	if (!found || acc.ballot <= req->ballot)
	{
		paxos_log_debug ("Storaged iid %u with thread_id %u a_tid %u",req->iid, acc.thread_id, acc.a_tid);
		paxos_log_debug("Preparing iid: %u, ballot: %u thread_id %u", req->iid, req->ballot, acc.thread_id);
		acc.aid = a->id;
		acc.iid = req->iid;
		acc.ballot = req->ballot;
		
		if (storage_put_record(&a->store[t_id], &acc) != 0) {
			storage_tx_abort(&a->store[t_id]);
			return 0;
		}
	}
	if (storage_tx_commit(&a->store[t_id]) != 0)
		return 0;
	paxos_accepted_to_promise(&acc, out);
	return 1;
}

int
acceptor_receive_accept(struct acceptor* a,
	paxos_accept* req, paxos_message* out, int t_id)
{
	paxos_log_debug("-------\n");
	paxos_log_debug("\nreceive accept message of thread_id %d with iid %u a->trim_iid[%d] %u", t_id,req->iid, t_id, a->trim_iid[t_id]);
	paxos_accepted acc;
	if (req->iid <= a->trim_iid[t_id])
		return 0;
	memset(&acc, 0, sizeof(paxos_accepted));
	if (storage_tx_begin(&a->store[t_id]) != 0)
		return 0;

	int found = storage_get_record(&a->store[t_id], req->iid, &acc);
	paxos_log_debug("accept found %d req->iid %u req->value_size %d",found,req->iid, req->value.paxos_value_len);
	if (!found || acc.ballot <= req->ballot) {
		paxos_log_debug("Accepting iid: %u, ballot: %u, thread_id %u", req->iid, req->ballot, req->thread_id);
		paxos_accept_to_accepted(a->id, req, out);
		if (storage_put_record(&a->store[t_id], &(out->u.accepted)) != 0) {
			storage_tx_abort(&a->store[t_id]);
			return 0;
		}
	} else {
		paxos_accepted_to_preempted(a->id, &acc, out);
	}
	if (storage_tx_commit(&a->store[t_id]) != 0)
		return 0;
	paxos_accepted_destroy(&acc);
	return 1;
}

int
acceptor_receive_repeat(struct acceptor* a, iid_t iid, paxos_accepted* out, int t_id)
{
	memset(out, 0, sizeof(paxos_accepted));
	if (storage_tx_begin(&a->store[t_id]) != 0)
		return 0;
	int found = storage_get_record(&a->store[t_id], iid, out);
	if (storage_tx_commit(&a->store[t_id]) != 0)
		return 0;
	return found && (out->value.paxos_value_len > 0);
}

int
acceptor_receive_trim(struct acceptor* a, paxos_trim* trim, int t_id)
{
	if (trim->iid <= a->trim_iid[t_id])
		return 0;
	a->trim_iid[t_id] = trim->iid;
	if (storage_tx_begin(&a->store[t_id]) != 0)
		return 0;
	storage_trim(&a->store[t_id], trim->iid);
	if (storage_tx_commit(&a->store[t_id]) != 0)
		return 0;
	return 1;
}

void
acceptor_set_current_state(struct acceptor* a, paxos_acceptor_state* state)
{
	state->aid = a->id;
	state->trim_iid = a->trim_iid[0];
}

static void
paxos_accepted_to_promise(paxos_accepted* acc, paxos_message* out)
{
	out->type = PAXOS_PROMISE;
	out->u.promise = (paxos_promise) {
		acc->iid,
		acc->ballot,
		acc->thread_id,
		acc->a_tid,
		acc->value_ballot,
		acc->aid,
		{acc->value.paxos_value_len, acc->value.paxos_value_val}
	};
	paxos_log_debug("paxos_accepted_to_promise iid %u ballot %u t_id %u a_tid %u value_ballot %u acceptor id %u, value %s",
		out->u.promise.iid,
		out->u.promise.ballot,
		out->u.promise.thread_id,
		out->u.promise.a_tid,
		out->u.promise.value_ballot,
		out->u.promise.aid,
		out->u.promise.value.paxos_value_val);
}

static void
paxos_accept_to_accepted(int id, paxos_accept* acc, paxos_message* out)
{
	char* value = NULL;
	int value_size = acc->value.paxos_value_len;
	paxos_log_debug("paxos_accept_to_accepted has value size %d", value_size);
	if (value_size > 0) {
		value = malloc(value_size);
		memcpy(value, acc->value.paxos_value_val, value_size);
	}
	out->type = PAXOS_ACCEPTED;
	out->u.accepted = (paxos_accepted) {
		acc->iid,
		acc->ballot,
		acc->thread_id,
		acc->a_tid,
		acc->ballot,
		id,
		{value_size, value}
	};
	paxos_log_debug("paxos_accept_to_accepted iid %u ballot %u t_id %u a_tid %u value_ballot %u acceptor id %u, value %s",
		out->u.accepted.iid,
		out->u.accepted.ballot,
		out->u.accepted.thread_id,
		out->u.accepted.a_tid,
		out->u.accepted.value_ballot,
		out->u.accepted.aid,
		out->u.accepted.value.paxos_value_val);

}

static void
paxos_accepted_to_preempted(int id, paxos_accepted* acc, paxos_message* out)
{
	paxos_log_debug("paxos_accepted_to_preempted");
	out->type = PAXOS_PREEMPTED;
	out->u.preempted = (paxos_preempted) {acc->iid, acc->ballot, acc->thread_id, acc->a_tid, 0, id, {0, NULL} };
}

