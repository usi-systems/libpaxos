#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <signal.h>
#include <errno.h>
#include <error.h>

#include "paxos.h"
#include "netutils.h"
#include "netpaxos.h"
#include "message_pack.h"

static void
sequencer_handle_proposal(struct paxos_ctx *ctx, char* buf, int size)
{
    uint16_t* be_threadid = (uint16_t *)(buf + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint16_t));
    uint32_t* inst_be = (uint32_t *)(buf + sizeof(uint16_t));
    uint16_t* be_acceptor_counter_id = (uint16_t *)(buf + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint16_t)+ + sizeof(uint16_t));
    uint16_t t =  ntohs(*be_threadid);

    //printf ("thread id %u\n",t);
    if (t == ALL)
    {
        int i;
        for (i = 0; i < NUM_OF_THREAD; i++)
        {
            *inst_be = htonl(ctx->sequencer->current_instance[i]++);
            paxos_log_debug("**All---thread_id_%u current_instance %u", i, ntohl(*inst_be));
            *be_threadid = htons(t);
            *be_acceptor_counter_id = htons(i);
            //paxos_log_debug("before sending, thread %u net_thread %u\n", ntohs(*be_threadid), *be_threadid);
            int n = sendto(ctx->sock, buf, size, 0,
                (struct sockaddr *)&ctx->acceptor_sin, sizeof(ctx->acceptor_sin));
            // paxos_log_debug("packet accept has size of %d\n", size); size 102
            if (n < 0)
                error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
        }
    }
    else
    {

        *inst_be = htonl(ctx->sequencer->current_instance[t]++);
        paxos_log_debug("**Specific---thread_id_%u current_instance %u", t, ntohl(*inst_be));
        *be_threadid = htons(t);
        *be_acceptor_counter_id = htons(t);
        paxos_log_debug("before sending, thread_id_%u aid %u", ntohs(*be_threadid), ntohs( *be_acceptor_counter_id));
        int n = sendto(ctx->sock, buf, size, 0,
                (struct sockaddr *)&ctx->acceptor_sin, sizeof(ctx->acceptor_sin));
        paxos_log_debug("packet accept has size of %d\n", size); //size 102
        if (n < 0)
            error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
    }
}


/*static void
handle_accepted(struct paxos_ctx *ctx, char* buf, int size, struct sockaddr_in *remote, size_t socklen)
{
    struct paxos_message msg;
    unpack_paxos_message(&msg, buf);
    char *val = (char*)msg.u.accepted.value.paxos_value_val;
    // Skip command ID and client address 
    char *retval = (val + sizeof(uint16_t) + sizeof(struct sockaddr_in));
    size_t retsize = size - sizeof(msg.u.accepted) + sizeof(uint16_t) + socklen;
     paxos_log_debug("packet accepted has size of %zd\n", retsize);
    int n = sendto(ctx->sock, retval, retsize, 0, (struct sockaddr *)remote, socklen);
    ctx->message_per_second++;
    if (n < 0)
        error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
}*/

static void
handle_packet_in(evutil_socket_t fd, short what, void *arg)
{
    struct paxos_ctx *ctx = arg;
    if (what&EV_READ) {
        char buf[BUFSIZE];
        struct sockaddr_in remote;
        socklen_t len = sizeof(remote);
        int n = recvfrom(fd, buf, BUFSIZE, 0,
                            (struct sockaddr *)&remote, &len);
        if (n < 0)
            error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
        uint16_t* msgtype_be = (uint16_t *)buf;
        uint16_t msgtype = ntohs(*msgtype_be);
        // hexdump(buf, n);
        switch(msgtype) {
            case PAXOS_ACCEPT:
                sequencer_handle_proposal(ctx, buf, n);
                break;
            case PAXOS_ACCEPTED: // use ACCEPTED for benchmarking
                //handle_accepted(ctx, buf, n, &remote, len);
                break;
            default:
                break;
        }
    }
}
/*void squencer_handle_accept(struct paxos_ctx *ctx, struct paxos_message *msg,
    struct sockaddr_in *remote, socklen_t socklen)
{
    //paxos_message out;
    paxos_accept* accept = &msg->u.accept;
    paxos_log_debug("paxos message has %d len %s value----paxos accept has %d len %s value",
                        msg->u.accept.value.paxos_value_len, 
                        msg->u.accept.value.paxos_value_val,
                        accept->value.paxos_value_len,
                        accept->value.paxos_value_val);
    int thread_id = accept->thread_id;
    //int acc_counter_id = accept->a_tid;
    if (thread_id == ALL)
    {
        int i;
        for (i = 0; i < NUM_OF_THREAD; i++)
        {
            struct paxos_message out = {
                .type = PAXOS_ACCEPT,
                .u.accept.iid = ctx->sequencer->current_instance[i]++,
                .u.accept.ballot = 0,
                .u.accept.thread_id  = thread_id,
                .u.accept.a_tid = i,
                .u.accept.value_ballot = 0,
                .u.accept.aid = 0,
                .u.accept.value.paxos_value_len = msg->u.accept.value.paxos_value_len,
                .u.accept.value.paxos_value_val = msg->u.accept.value.paxos_value_val
            };

            size_t msg_len = pack_paxos_message(ctx->buffer, &out);
            int n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
                    (struct sockaddr *)&ctx->acceptor_sin, sizeof(ctx->acceptor_sin));
            
            if (n < 0)
            error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
    
        }
    }
    else if (thread_id == 0 || thread_id == 1){
        struct paxos_message out = {
                .type = PAXOS_ACCEPT,
                .u.accept.iid =  ctx->sequencer->current_instance[thread_id]++,
                .u.accept.ballot = 0,
                .u.accept.thread_id  = thread_id,
                .u.accept.a_tid = thread_id,
                .u.accept.value_ballot = 0,
                .u.accept.aid = 0,
                .u.accept.value.paxos_value_len = msg->u.accept.value.paxos_value_len,
                .u.accept.value.paxos_value_val = msg->u.accept.value.paxos_value_val
            };

        paxos_log_debug("send paxos message iid %u ballot %u thread_id_%u acceptor_counter_id %u"
            "len %d value %s",
            out.u.accept.iid, 
            out.u.accept.ballot,
            out.u.accept.thread_id,
            out.u.accept.a_tid,
            out.u.accept.value.paxos_value_len,
            out.u.accept.value.paxos_value_val);

        char* val = out.u.accept.value.paxos_value_val;
            //size_t size = out.u.accept.value.paxos_value_len;
        struct command *cmd = (struct command*)(val + sizeof(struct client_request) - 1);
        char *key = cmd->content;
        char *value = cmd->content + 16;
        paxos_log_debug("SET(%s, %s) thread_id_%d msg->thread_id_%u iid %u command_id_%u client_id_%u\n", 
                                        key, value, 
                                        cmd->thread_id,
                                        out.u.accept.thread_id,
                                        out.u.accept.iid,
                                        cmd->command_id, 
                                        cmd->client_id);

        size_t msg_len = pack_paxos_message(ctx->buffer, &out);
        int n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
                        (struct sockaddr *)&ctx->acceptor_sin, sizeof(ctx->acceptor_sin));
            
        if (n < 0)
            error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
    }
    
}*/
/*static void
handle_packet(evutil_socket_t fd, short what, void *arg)
{
   
    struct paxos_ctx *ctx = arg;
    if (what&EV_READ) {
        memset(ctx->buffer, 0, BUFSIZE);
        struct sockaddr_in remote;
        socklen_t len = sizeof(remote);

        int n = recvfrom(fd, ctx->buffer, BUFSIZE, 0,
                            (struct sockaddr *)&remote, &len);
        if (n < 0)
            perror("recvfrom");

        struct paxos_message msg;
        unpack_paxos_message(&msg, ctx->buffer);
        if (msg.type == PAXOS_ACCEPT) {
            squencer_handle_accept(ctx, &msg, &remote, len);
        }
         paxos_message_destroy(&msg);
     }

}*/
struct sequencer*
sequencer_new() {
    struct sequencer* sq = malloc(sizeof(struct sequencer) + (NUM_OF_THREAD * sizeof(uint32_t)));
    int i;
    for (i = 0; i < NUM_OF_THREAD; i++)
    {
       sq->current_instance[i] = 1;
    }
    
    return sq;
}

struct paxos_ctx *make_sequencer(struct netpaxos_configuration *conf)
{
    struct paxos_ctx *ctx = malloc(sizeof(struct paxos_ctx));
    init_paxos_ctx(ctx);

    ctx->sequencer = sequencer_new();

    ctx->base = event_base_new();

    evutil_socket_t sock = create_server_socket(conf->coordinator_port);
    evutil_make_socket_nonblocking(sock);
    ctx->sock = sock;

    if (net_ip__is_multicast_ip(conf->coordinator_address)) {
        subcribe_to_multicast_group(conf->coordinator_address, sock);
    }

    ip_to_sockaddr(conf->acceptor_address, conf->acceptor_port,
                    &ctx->acceptor_sin);

    ctx->ev_read = event_new(ctx->base, sock, EV_READ|EV_PERSIST,
        handle_packet_in, ctx);
    event_add(ctx->ev_read, NULL);

    ctx->ev_sigint = evsignal_new(ctx->base, SIGINT, handle_signal, ctx);
    evsignal_add(ctx->ev_sigint, NULL);

    ctx->ev_sigterm = evsignal_new(ctx->base, SIGTERM, handle_signal, ctx);
    evsignal_add(ctx->ev_sigterm, NULL);

    struct event *ev_perf = event_new(ctx->base, -1, EV_TIMEOUT|EV_PERSIST, on_perf, ctx);
    struct timeval one_second = {1, 0};
    event_add(ev_perf, &one_second);

    return ctx;
}
