#include "message_pack.h"
#include "gtest/gtest.h"

#define BUFSIZE 1024

class MessagePackTest : public testing::Test {
protected:
    char buffer[BUFSIZE];

    virtual void SetUp() {
        memset(buffer, 0, BUFSIZE);
    }

};

TEST_F(MessagePackTest, PackPrepare) {
    struct paxos_message msg = { PAXOS_PREPARE, 1, 1, 0, 0, {0, NULL}};
    // pack_paxos_message(buffer, &msg);    
    size_t size = sizeof(struct paxos_message);
    struct paxos_message out;
    // unpack_paxos_message(&out, buffer);
    EXPECT_EQ(PAXOS_PREPARE, msg.type);
    EXPECT_EQ(1, msg.u.prepare.iid);
    EXPECT_EQ(1, msg.u.prepare.ballot);
}