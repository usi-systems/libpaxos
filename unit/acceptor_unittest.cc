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


#include "acceptor.h"
#include "gtest/gtest.h"

class AcceptorTest : public::testing::TestWithParam<paxos_storage_backend> {
protected:

	int id;
	struct acceptor* a;
	
	virtual void SetUp() {
		id = 2;
		paxos_config.verbosity = PAXOS_LOG_QUIET;
		paxos_config.storage_backend = GetParam();
		paxos_config.trash_files = 1;
		a = acceptor_new(id);
	}
	
	virtual void TearDown() {
		acceptor_free(a);
	}
};

void CHECK_PROMISE(paxos_message msg, int iid, int bal, int vbal, char *val) {            \
	ASSERT_EQ(msg.type, PAXOS_PROMISE);
	ASSERT_EQ(msg.u.promise.iid, iid);
	ASSERT_EQ(msg.u.promise.ballot, bal);
	ASSERT_EQ(msg.u.promise.value_ballot, vbal);
	ASSERT_EQ(msg.u.promise.value.paxos_value_len, 
		val == NULL ? 0 : strlen(val)+1);
	ASSERT_STREQ(msg.u.promise.value.paxos_value_val, val);
}

void CHECK_ACCEPTED(paxos_message msg, int id, int bal, int vbal, char *val) {            \
	ASSERT_EQ(msg.type, PAXOS_ACCEPTED);
	ASSERT_EQ(msg.u.accepted.iid, id);
	ASSERT_EQ(msg.u.accepted.ballot, bal);
	ASSERT_EQ(msg.u.accepted.value_ballot, vbal);
	ASSERT_EQ(msg.u.accepted.value.paxos_value_len,
		val == NULL ? 0 : strlen(val)+1);
	ASSERT_STREQ(msg.u.accepted.value.paxos_value_val, val);
}

void CHECK_PREEMPTED(paxos_message msg, int id, int bal) {
	ASSERT_EQ(msg.type, PAXOS_PREEMPTED);
	ASSERT_EQ(msg.u.preempted.iid, id);
	ASSERT_EQ(msg.u.preempted.ballot, bal);
}


TEST_P(AcceptorTest, Prepare) {
	paxos_message msg;
	paxos_prepare pre = {1, 101, 0, 0, {0, NULL}};
	acceptor_receive_prepare(a, &pre, &msg);
	CHECK_PROMISE(msg, 1, 101, 0, NULL);
}

TEST_P(AcceptorTest, PrepareDuplicate) {
	paxos_message msg;
	paxos_prepare pre = {1, 101, 101, id, {0, NULL}};
	acceptor_receive_prepare(a, &pre, &msg);
	acceptor_receive_prepare(a, &pre, &msg);
	CHECK_PROMISE(msg, 1, 101, 0, NULL);
}

TEST_P(AcceptorTest, PrepareSmallerBallot) {
	paxos_message msg;
	int ballots[] = {11, 5, 9, 10, 2};
	for (int i = 0; i < (sizeof(ballots)/sizeof(int)); ++i) {
		paxos_prepare pre = {1, ballots[i], 0, 0, {0, NULL}};
		acceptor_receive_prepare(a, &pre, &msg);
		CHECK_PROMISE(msg, 1, ballots[0], 0, NULL);
	}
}

TEST_P(AcceptorTest, PrepareHigherBallot) {
	paxos_message msg;
	int ballots[] = {0, 10, 11, 20, 33};
	for (int i = 0; i < sizeof(ballots)/sizeof(int); ++i) {
		paxos_prepare pre = (paxos_prepare) {1, ballots[i], 0, 0, {0, NULL}};
		acceptor_receive_prepare(a, &pre, &msg);
		CHECK_PROMISE(msg, 1, ballots[i], 0, NULL);
	}
}

TEST_P(AcceptorTest, Accept) {
	paxos_message msg;
	char value[] = "foo";
	paxos_accept ar = {1, 101, 101, 0, {4, value}};
	acceptor_receive_accept(a, &ar, &msg);
	CHECK_ACCEPTED(msg, 1, 101, 101, value);
	paxos_message_destroy(&msg);
}

TEST_P(AcceptorTest, AcceptPrepared) {
	char value[] = "foo bar";
	paxos_prepare pr = {1, 101, 0, 0, {0, NULL}};
	paxos_accept ar = {1, 101, 101, 0, {8 , value}};
	paxos_message msg;

	acceptor_receive_prepare(a, &pr, &msg);
	CHECK_PROMISE(msg, 1, 101, 0, NULL);

	acceptor_receive_accept(a, &ar, &msg);
	CHECK_ACCEPTED(msg, 1, 101, 101, value);
	paxos_message_destroy(&msg);
}

TEST_P(AcceptorTest, AcceptHigherBallot) {
	char value[] = "baz";
	paxos_prepare pr = {1, 101, 0, 0, {0, NULL}};
	paxos_accept ar = {1, 201, 201, 0, {4, value}};
	paxos_message msg;

	acceptor_receive_prepare(a, &pr, &msg);
	CHECK_PROMISE(msg, 1, 101, 0, NULL);

	acceptor_receive_accept(a, &ar, &msg);
	CHECK_ACCEPTED(msg, 1, 201, 201, value);
	paxos_message_destroy(&msg);
}

TEST_P(AcceptorTest, AcceptSmallerBallot) {
	paxos_accepted acc;
	int ballot = 201;
	int acceptor_id = 0;
	char value[] = "bar";
	int vsize = sizeof(value);
	paxos_prepare pr = {1, ballot, ballot, acceptor_id, {0, NULL}};
	paxos_accept ar = {1, ballot - 1, ballot, acceptor_id, {vsize, value}};
	paxos_message msg;
	acceptor_receive_prepare(a, &pr, &msg);
	CHECK_PROMISE(msg, 1, ballot, 0, NULL);

	acceptor_receive_accept(a, &ar, &msg);
	CHECK_PREEMPTED(msg, 1, ballot);
	paxos_message_destroy(&msg);
}

TEST_P(AcceptorTest, PrepareWithAcceptedValue) {
	paxos_accepted acc;
	int ballot = 101;
	int value_ballot = 101;
	int acceptor_id = 0;
	char value[] = "bar";
	int vsize = sizeof(value);
	paxos_prepare pr = {1, ballot, value_ballot, acceptor_id, {0, NULL}};
	paxos_accept ar = {1, ballot, value_ballot, acceptor_id, {vsize, value}};
	paxos_message msg;
	acceptor_receive_prepare(a, &pr, &msg);
	acceptor_receive_accept(a, &ar, &msg);
	paxos_message_destroy(&msg);

	pr = (paxos_prepare) {1, 201, 0, 0, {0, NULL}};
	acceptor_receive_prepare(a, &pr, &msg);
	CHECK_PROMISE(msg, 1, 201, 101, value);
	paxos_message_destroy(&msg);
}

TEST_P(AcceptorTest, Repeat) {
	paxos_message msg;
	paxos_accepted acc;
	int ballot = 101;
	int value_ballot = 101;
	int acceptor_id = 0;
	char value[] = "1234";
	int vsize = sizeof(value);
	paxos_accept ar = {10, ballot, value_ballot, acceptor_id, {vsize, value}};
	acceptor_receive_accept(a, &ar, &msg);
	paxos_message_destroy(&msg);
	ASSERT_TRUE(acceptor_receive_repeat(a, 10, &acc));
	paxos_accepted_destroy(&acc);
}

TEST_P(AcceptorTest, RepeatEmpty) {
	paxos_accepted acc;
	ASSERT_FALSE(acceptor_receive_repeat(a, 1, &acc));
}

TEST_P(AcceptorTest, RepeatPrepared) {
	paxos_accepted acc;
	paxos_prepare pre = {1, 101};
	paxos_message msg;

	acceptor_receive_prepare(a, &pre, &msg);
	paxos_message_destroy(&msg);
	ASSERT_FALSE(acceptor_receive_repeat(a, 1, &acc));
}

TEST_P(AcceptorTest, TrimmedInstances) {
	paxos_message msg;
	int ballot = 101;
	int value_ballot = 101;
	int acceptor_id = 2;
	char value[] = "1234";
	int vsize = sizeof(value);
	// in-memory storage does not support trimming
	if (paxos_config.storage_backend == PAXOS_MEM_STORAGE)
		return;

	paxos_accept ar1 = {0, ballot, value_ballot, acceptor_id, {vsize, value}};
	acceptor_receive_accept(a, &ar1, &msg);
	paxos_message_destroy(&msg);
	
	paxos_accept ar2 = {0, ballot, value_ballot, acceptor_id + 1, {vsize, value}};
	acceptor_receive_accept(a, &ar2, &msg);
	paxos_message_destroy(&msg);
	
	
	// acceptors should not prepare/accept/repeat trimmed instances
	paxos_trim trim = {5};
	acceptor_receive_trim(a, &trim);
	
	paxos_prepare pre;
	for (int i = 1; i < 6; ++i) {
		pre = (paxos_prepare){i, ballot, value_ballot, acceptor_id, {vsize, value}};
		ASSERT_FALSE(acceptor_receive_prepare(a, &pre, &msg));
	}
	
	paxos_accept acc;
	for (int i = 1; i < 6; ++i) {
		acc = (paxos_accept){i, ballot, value_ballot, acceptor_id, {vsize, (char*)"test"}};;
		ASSERT_FALSE(acceptor_receive_accept(a, &acc, &msg));
	}
	
	paxos_accepted accepted;
	for (int i = 1; i < 6; ++i) {
		ASSERT_FALSE(acceptor_receive_repeat(a, i, &accepted));
	}
}


const paxos_storage_backend backends[] = {
	PAXOS_MEM_STORAGE,
#if HAS_LMDB
	PAXOS_LMDB_STORAGE,
#endif
};

INSTANTIATE_TEST_CASE_P(StorageBackends, AcceptorTest, 
	testing::ValuesIn(backends));
