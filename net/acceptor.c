#include "acceptor.h"
#include "paxos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>
#include <signal.h>
#include "netutils.h"
#include "netpaxos.h"
#include "message_pack.h"
#include <errno.h>
#include <error.h>


void acceptor_handle_prepare(struct paxos_ctx *ctx, struct paxos_message *msg,
        struct sockaddr_in *remote, socklen_t socklen)
{
    paxos_message out;
    paxos_prepare* prepare = &msg->u.prepare;
    uint16_t thread_id = prepare->thread_id;
    int acc_counter_id = prepare->a_tid;
    paxos_log_debug("-------\n");
    paxos_log_debug("acceptor_handle_prepare iid %u thread_id %u acc_counter_id %d\n",prepare->iid, thread_id, acc_counter_id);
    if (thread_id == ALL)
    {
        if (acceptor_receive_prepare(ctx->acceptor_state, prepare, &out, acc_counter_id) != 0)
        {
            size_t msg_len = pack_paxos_message(ctx->buffer, &out);
            int n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
                (struct sockaddr *)remote, socklen);
            if (n < 0)
                error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
            paxos_message_destroy(&out);
        }
            
    }
    else
    {
        if (acceptor_receive_prepare(ctx->acceptor_state, prepare, &out, thread_id) != 0)
        {
            size_t msg_len = pack_paxos_message(ctx->buffer, &out);
            int n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
                (struct sockaddr *)remote, socklen);
            if (n < 0)
                error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
            paxos_message_destroy(&out);
        }
    }
    
    
}

void acceptor_handle_accept(struct paxos_ctx *ctx, struct paxos_message *msg,
    struct sockaddr_in *remote, socklen_t socklen)
{
    paxos_message out;
    paxos_accept* accept = &msg->u.accept;
    //paxos_log_debug("*thread_id  %d\n", accept->thread_id);
    int thread_id = accept->thread_id;
    int acc_counter_id = accept->a_tid;
    if (thread_id == ALL)
    {
        if (acceptor_receive_accept(ctx->acceptor_state, accept, &out, acc_counter_id) != 0)
        {

            if (out.type == PAXOS_ACCEPTED)
            {
                size_t msg_len = pack_paxos_message(ctx->buffer, &out);
                int n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
                            (struct sockaddr *)&ctx->learner_sin[acc_counter_id], 
                            sizeof(ctx->learner_sin[acc_counter_id]));
                paxos_log_debug("-1- ALL: send to learner: thread_id %d, instance id %u a_id %d \n", 
                            acc_counter_id, out.u.accept.iid, out.u.accept.aid) ;
                if (n < 0)
                    error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
               
                n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
                            (struct sockaddr *)remote, socklen);
                if (n < 0)
                    error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
              
            }  
             paxos_message_destroy(&out);
        }
    }
    else
    {
        if (acceptor_receive_accept(ctx->acceptor_state, accept, &out, thread_id) != 0)
        {
            if (out.type == PAXOS_ACCEPTED)
            {
                size_t msg_len = pack_paxos_message(ctx->buffer, &out);
                int n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
                        (struct sockaddr *)&ctx->learner_sin[thread_id], sizeof(ctx->learner_sin[thread_id]));
                paxos_log_debug("-2- send to learner: thread_id  %d, instance id %u a_id %u",
                         thread_id, out.u.accept.iid, out.u.accept.aid);
                if (n < 0)
                    error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
            
                n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
                            (struct sockaddr *)remote, socklen);
                if (n < 0)
                    error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));    
            }
            paxos_message_destroy(&out);  
        }
    }
}

void acceptor_handle_repeat(struct paxos_ctx *ctx, struct paxos_message* msg,
    struct sockaddr_in *remote, socklen_t socklen)
{
    iid_t iid;
    paxos_message out;
    out.type = PAXOS_ACCEPTED;
    paxos_repeat* repeat = &msg->u.repeat;
    int thread_id = repeat->thread_id;
    int acc_counter_id = repeat->a_tid;
    if (thread_id == ALL)
    {
        paxos_log_debug("Handle repeat for iids %d-%d of combined thread id %d", repeat->from, repeat->to, acc_counter_id);
        for (iid = repeat->from; iid <= repeat->to; ++iid)
        {
            if (acceptor_receive_repeat(ctx->acceptor_state, iid, &out.u.accepted, acc_counter_id)) {
                size_t msg_len = pack_paxos_message(ctx->buffer, &out);
                int n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
                        (struct sockaddr *)remote, socklen);
                if (n < 0)
                    error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
                paxos_message_destroy(&out);
            }
        } 
    }
    else
    {
       paxos_log_debug("Handle repeat for iids %d-%d of thread id %d", repeat->from, repeat->to, thread_id);
        for (iid = repeat->from; iid <= repeat->to; ++iid)
        {
            if (acceptor_receive_repeat(ctx->acceptor_state, iid, &out.u.accepted,thread_id)) {
                size_t msg_len = pack_paxos_message(ctx->buffer, &out);
                int n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
                    (struct sockaddr *)remote, socklen);
                if (n < 0)
                    error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
                paxos_message_destroy(&out);
            }
        } 
    }
    
}

void
acceptor_handle_benchmark(struct paxos_ctx *ctx, struct paxos_message *msg, int size, struct sockaddr_in *remote, size_t socklen)
{
    char *val = (char*)msg->u.accepted.value.paxos_value_val;
    /* Skip command ID and client address */
    char *retval = (val + sizeof(uint16_t) + sizeof(struct sockaddr_in));
    size_t retsize = size - sizeof(msg->u.accepted) + sizeof(uint16_t) + socklen;
    int n = sendto(ctx->sock, retval, retsize, 0,
        (struct sockaddr *)remote, socklen);
    ctx->message_per_second++;
    if (n < 0)
        error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
}
void acceptor_read(evutil_socket_t fd, short what, void *arg)
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
            acceptor_handle_accept(ctx, &msg, &remote, len);
        } else if (msg.type == PAXOS_PREPARE) {
            acceptor_handle_prepare(ctx, &msg, &remote, len);
        } else if (msg.type == PAXOS_REPEAT) {
            acceptor_handle_repeat(ctx, &msg, &remote, len);
        } else if (msg.type == PAXOS_ACCEPTED) { // use ACCEPTED for benchmarking
            //acceptor_handle_benchmark(ctx, &msg, n, &remote, len);
        }

        paxos_message_destroy(&msg);
    }
}


struct paxos_ctx *make_acceptor(struct netpaxos_configuration *conf, int aid)
{
    struct paxos_ctx *ctx = malloc( sizeof(struct paxos_ctx));
    init_paxos_ctx(ctx);

    ctx->acceptor_state = acceptor_new(aid);

    ctx->base = event_base_new();

    evutil_socket_t sock = create_server_socket(conf->acceptor_port);
    evutil_make_socket_nonblocking(sock);
    ctx->sock = sock;

    if (net_ip__is_multicast_ip(conf->acceptor_address)) {
        subcribe_to_multicast_group(conf->acceptor_address, sock);
    }

    // ip_to_sockaddr(conf->learner_address, conf->learner_port, &ctx->learner_sin);
    int i;
    for (i = 0; i < conf->learner_count ; i++)
    {
         ip_to_sockaddr( conf->learner_address[i],
                    conf->learner_port[i],
                    &ctx->learner_sin[i]);
    }

    ip_to_sockaddr( conf->coordinator_address, conf->coordinator_port,
                    &ctx->coordinator_sin );

    ctx->ev_read = event_new(ctx->base, sock, EV_READ|EV_PERSIST,
        acceptor_read, ctx);
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
