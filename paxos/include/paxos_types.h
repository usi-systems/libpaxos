/*
 * Copyright (c) 2013-2015, University of Lugano
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


#ifndef _PAXOS_TYPES_H_
#define _PAXOS_TYPES_H_

#include <stdint.h>

struct __attribute__((__packed__)) paxos_hole
{
	int paxos_hole_len;
	uint32_t *paxos_hole_iid;
};
typedef struct paxos_hole paxos_hole;

struct __attribute__((__packed__)) paxos_thread
{
	uint16_t paxos_thread_len;
	uint16_t *paxos_thread_iid;
};
typedef struct paxos_thread paxos_thread;

struct __attribute__((__packed__)) paxos_value
{
	int paxos_value_len;
	char *paxos_value_val;
};
typedef struct paxos_value paxos_value;

struct __attribute__((__packed__)) paxos_prepare
{
	uint32_t iid;
	uint16_t ballot;
	uint16_t thread_id;
	uint16_t a_tid;
	uint16_t value_ballot;
	uint16_t aid;
	paxos_value value;
};
typedef struct paxos_prepare paxos_prepare;

struct __attribute__((__packed__)) paxos_prepare_hole
{
	uint32_t iid;
	uint16_t ballot;
	uint16_t thread_id;
	uint16_t a_tid;
	uint16_t value_ballot;
	uint16_t aid;
	paxos_value value;
	paxos_hole hole;
};
typedef struct paxos_prepare_hole paxos_prepare_hole;

struct __attribute__((__packed__)) paxos_promise
{
	uint32_t iid;
	uint16_t ballot;
	uint16_t thread_id;
	uint16_t a_tid;
	uint16_t value_ballot;
	uint16_t aid;
	paxos_value value;
};
typedef struct paxos_promise paxos_promise;

struct __attribute__((__packed__)) paxos_promise_hole
{
	uint32_t iid;
	uint16_t ballot;
	uint16_t thread_id;
	uint16_t a_tid;
	uint16_t value_ballot;
	uint16_t aid;
	paxos_value value;
};
typedef struct paxos_promise_hole paxos_promise_hole;

struct __attribute__((__packed__)) paxos_accept
{
	uint32_t iid;
	uint16_t ballot;
	uint16_t thread_id;
	uint16_t a_tid;
	uint16_t value_ballot;
	uint16_t aid;
	paxos_value value;
};
typedef struct paxos_accept paxos_accept;
struct __attribute__((__packed__)) paxos_accept_hole
{
	uint32_t iid;
	uint16_t ballot;
	uint16_t thread_id;
	uint16_t a_tid;
	uint16_t value_ballot;
	uint16_t aid;
	paxos_value value;
};
typedef struct paxos_accept_hole paxos_accept_hole;

struct __attribute__((__packed__)) paxos_accepted
{
	uint32_t iid;
	uint16_t ballot;
	uint16_t thread_id;
	uint16_t a_tid;
	uint16_t value_ballot;
	uint16_t aid;
	paxos_value value;
};
typedef struct paxos_accepted paxos_accepted;

struct __attribute__((__packed__)) paxos_accepted_hole
{
	uint32_t iid;
	uint16_t ballot;
	uint16_t thread_id;
	uint16_t a_tid;
	uint16_t value_ballot;
	uint16_t aid;
	paxos_value value;
};
typedef struct paxos_accepted_hole paxos_accepted_hole;

struct __attribute__((__packed__)) paxos_preempted
{
	uint32_t iid;
	uint16_t ballot;
	uint16_t thread_id;
	uint16_t a_tid;
	uint16_t value_ballot;
	uint16_t aid;	
	paxos_value value;
};
typedef struct paxos_preempted paxos_preempted;

struct __attribute__((__packed__)) paxos_repeat
{
	uint32_t from;
	uint32_t to;
	uint16_t thread_id;
	uint16_t a_tid;
};
typedef struct paxos_repeat paxos_repeat;

struct __attribute__((__packed__)) paxos_trim
{
	uint32_t iid;
};
typedef struct paxos_trim paxos_trim;

struct __attribute__((__packed__)) paxos_acceptor_state
{
	uint32_t aid;
	uint32_t trim_iid;
};
typedef struct paxos_acceptor_state paxos_acceptor_state;

struct __attribute__((__packed__)) paxos_client_value
{
	paxos_value value;
};
typedef struct paxos_client_value paxos_client_value;

enum paxos_message_type
{
	PAXOS_PREPARE,
	PAXOS_PROMISE,
	PAXOS_ACCEPT,
	PAXOS_ACCEPTED,
	PAXOS_PREEMPTED,
	PAXOS_REPEAT,
	PAXOS_TRIM,
	PAXOS_ACCEPTOR_STATE,
	PAXOS_CLIENT_VALUE,
	PAXOS_PREPARE_HOLE,
	PAXOS_PROMISE_HOLE,
	PAXOS_ACCEPT_HOLE,
};
typedef enum paxos_message_type paxos_message_type;

struct __attribute__((__packed__)) paxos_message
{
	paxos_message_type type;
	union
	{
		paxos_prepare prepare;
		paxos_promise promise;
		paxos_accept accept;
		paxos_accepted accepted;
		paxos_preempted preempted;
		paxos_repeat repeat;
		paxos_trim trim;
		paxos_acceptor_state state;
		paxos_client_value client_value;
		paxos_prepare_hole prepare_hole;
	} u;
};
typedef struct paxos_message paxos_message;

#endif
