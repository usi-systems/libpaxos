/*
	Copyright (c) 2013, University of Lugano
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
    	* Redistributions of source code must retain the above copyright
		  notice, this list of conditions and the following disclaimer.
		* Redistributions in binary form must reproduce the above copyright
		  notice, this list of conditions and the following disclaimer in the
		  documentation and/or other materials provided with the distribution.
		* Neither the name of the copyright holders nor the
		  names of its contributors may be used to endorse or promote products
		  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.	
*/


#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "paxos.h"
#include <stdlib.h>

struct storage
{
	void* handle;
	struct
	{
		int (*open) (void* handle);
		int (*close) (void* handle);
		void (*tx_begin) (void* handle);
		void (*tx_commit) (void* handle);
		int (*get) (void* handle, iid_t iid, paxos_accepted* out);
		int (*put) (void* handle, paxos_accepted* acc);
	} api;
};

void storage_init_bdb(struct storage* s, int acceptor_id);
void storage_init_mem(struct storage* s, int acceptor_id);


static void
storage_init(struct storage* store, int acceptor_id)
{
	switch(paxos_config.storage_backend) {
		#ifdef HAS_BDB
		case PAXOS_BDB_STORAGE:
			storage_init_bdb(store, acceptor_id);
			break;
		#endif
		case PAXOS_MEM_STORAGE:
			storage_init_mem(store, acceptor_id);
			break;
		default:
		paxos_log_error("Storage backend not available");
		exit(0);
	}
}

static int
storage_open(struct storage* store)
{
	return store->api.open(store->handle);
}

static int
storage_close(struct storage* store)
{
	return store->api.close(store->handle);
}

static void
storage_tx_begin(struct storage* store)
{
	store->api.tx_begin(store->handle);
}

static void
storage_tx_commit(struct storage* store)
{
	store->api.tx_commit(store->handle);
}

static int
storage_get_record(struct storage* store, iid_t iid, paxos_accepted* out)
{
	return store->api.get(store->handle, iid, out);
}

static int
storage_put_record(struct storage* store, paxos_accepted* acc)
{
	return store->api.put(store->handle, acc);
}

#endif
