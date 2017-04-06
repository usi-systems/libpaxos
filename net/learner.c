#define _GNU_SOURCE
#include "learner.h"
#include "paxos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <signal.h>
#include <time.h>
#include "netutils.h"
#include "netpaxos.h"
#include "message_pack.h"

#define MAX_GAPS 20
#define TIMEOUT 1
int counter = 0;

static void
send_paxos_message(struct learner_thread *l, paxos_message* msg, int msg_size){
    char buffer[BUFSIZE];
    pack_paxos_message(buffer, msg);
    sendto(l->ctx->sock, buffer, msg_size, 0,
            (struct sockaddr*)&(l->ctx->acceptor_sin), sizeof(l->ctx->acceptor_sin));
    // print_addr(&ctx->acceptor_sin);
}


void check_holes(evutil_socket_t fd, short event, void *arg)
{
    struct learner_thread *l = (struct learner_thread *)arg;
    unsigned from_inst;
    unsigned to_inst;
    int message_size = 0;

    if (learner_has_holes(l->ctx->learner_state, &from_inst, &to_inst))
    {
        paxos_log_debug("learner_id_%u has holes from %d to %d", l->lth_id, from_inst, to_inst);
        paxos_message out;
        out.type = PAXOS_PREPARE_HOLE;
        if ((to_inst - from_inst) > MAX_GAPS)
            to_inst = from_inst + MAX_GAPS;
        learner_prepare_hole (l->ctx->learner_state, &out.u.prepare_hole, &from_inst, &to_inst, l->lth_id, &message_size);
        paxos_log_debug("learner_id_%u send prepare message for iid %u ballot %u thread_id_%u a_tid %u value_ballot %u",
            l->lth_id,
            out.u.prepare_hole.iid,
            out.u.prepare_hole.ballot,
            out.u.prepare_hole.thread_id,
            out.u.prepare_hole.a_tid,
            out.u.prepare_hole.value_ballot);
        send_paxos_message(l, &out, message_size); 
    }
}

void on_paxos_accepted(paxos_message *msg, struct learner_thread *l_th) {

    uint16_t tid = l_th->lth_id;
    if(msg->u.accepted.a_tid != tid)
    {   
        paxos_log_debug("**thread_id_%u in message iid %u vs learner_id_%u",
         msg->u.accepted.a_tid, msg->u.accepted.iid, tid);
        return;
    }


    learner_receive_accepted(l_th->ctx->learner_state, &msg->u.accepted, tid);

    paxos_accepted chosen_value;
    memset(&chosen_value, 0, sizeof(paxos_accepted));
    paxos_log_debug("---------------------");
   
    while (learner_deliver_next(l_th->ctx->learner_state, &chosen_value, tid)) {
       
        if (chosen_value.thread_id == ALL)
        {
            pthread_mutex_lock (&execute_mutex);
            counter ++;
            
            if (counter == NUM_OF_THREAD)
            {
                paxos_log_debug("learner_id_%u reached", tid);
                paxos_log_debug("-1 learner_id_%u execute iid %u of thread_id_%u len %d value %s", 
                    tid,
                    chosen_value.iid, chosen_value.thread_id, 
                    chosen_value.value.paxos_value_len,
                    chosen_value.value.paxos_value_val );
                counter = 0;
                l_th->ctx->deliver(
                    tid,
                    chosen_value.iid,
                    chosen_value.value.paxos_value_val,
                    chosen_value.value.paxos_value_len,
                    l_th->ctx->deliver_arg);
                paxos_accepted_destroy(&chosen_value);
                pthread_cond_broadcast(&execute);
            }
            else{

                paxos_log_debug("learner_id_%d wait",tid);
                pthread_cond_wait(&execute, &execute_mutex);
        
            }
            pthread_mutex_unlock(&execute_mutex);
        }
        else{
            paxos_log_debug("-2 learner_id_%u execute iid %u of thread_id_%u len %d value %s", 
                    tid,
                    chosen_value.iid, 
                    chosen_value.thread_id,
                    chosen_value.value.paxos_value_len,
                    chosen_value.value.paxos_value_val);
            l_th->ctx->deliver(
                    tid,
                    chosen_value.iid,
                    chosen_value.value.paxos_value_val,
                    chosen_value.value.paxos_value_len,
                    l_th->ctx->deliver_arg);
            paxos_accepted_destroy(&chosen_value);
        }  
    }
    

}

void on_paxos_promise(paxos_message *msg, struct learner_thread *l) {
    int ret;
    paxos_message out;
    out.type = PAXOS_ACCEPT;
    ret = learner_receive_promise(l->ctx->learner_state, &msg->u.promise, &out.u.accept);
    if (ret) {
        size_t datagram_len = sizeof(paxos_message) + msg->u.promise.value.paxos_value_len;
        send_paxos_message(l, &out, datagram_len);
    }
}
void on_paxos_promise_hole(paxos_message *msg, struct learner_thread *l) {
    int ret;
    paxos_message out;
    out.type = PAXOS_ACCEPT_HOLE;
    paxos_log_debug("---promise hole---");
    ret = learner_receive_promise(l->ctx->learner_state, &msg->u.promise, &out.u.accept);
    if (ret) {
        size_t datagram_len = sizeof(paxos_message) + msg->u.promise.value.paxos_value_len;
        send_paxos_message(l, &out, datagram_len);
    }
}
void on_paxos_preempted(paxos_message *msg, struct learner_thread *l) {
    int ret;
    paxos_message out;
    out.type = PAXOS_PREPARE;
    ret = learner_receive_preempted(l->ctx->learner_state, &msg->u.preempted, &out.u.prepare);
    if (ret) {
        size_t datagram_len = sizeof(paxos_message);
        send_paxos_message(l, &out, datagram_len);
    }
}

void learner_read_cb(evutil_socket_t fd, short what, void *arg) {
    //printf("read\n");
    struct learner_thread *l = arg;
    if (what&EV_READ) {
        int retval;
        int i;
        retval = recvmmsg(fd, l->msgs, VLEN, 0, &l->timeout);
        if (retval == -1)
        {
            perror("recvmmsg()");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < retval; i++) {

            l->bufs[i][l->msgs[i].msg_len] = 0;
            
            if (l->max_received < retval) 
            {
                l->max_received = retval;
                // printf("max_received %d\n", max_received);
            }
            struct paxos_message msg;
            unpack_paxos_message(&msg, l->bufs[i]);

            switch(msg.type) {
                case PAXOS_ACCEPTED:
                    on_paxos_accepted(&msg, l);
                    break;
                case PAXOS_PROMISE:
                    on_paxos_promise(&msg, l);
                    break;
                case PAXOS_PROMISE_HOLE:
                    on_paxos_promise_hole(&msg, l);
                    break;
                case PAXOS_PREEMPTED:
                    on_paxos_preempted(&msg, l);
                    break;
                default:
                    paxos_log_debug("No handler for message type %d\n", msg.type);
            }
            paxos_message_destroy(&msg);
        }

    }
}


struct learner_thread*
make_learner (int learner_id, struct netpaxos_configuration *conf, deliver_function f, void *arg)
{
    struct learner_thread* l;
    l = malloc (sizeof(struct learner_thread));

    l->ctx = malloc(sizeof(struct paxos_ctx));
    init_paxos_ctx(l->ctx);

    l->ctx->base = event_base_new();
    l->lth_id = learner_id;
    evutil_socket_t sock = create_server_socket(conf->learner_port[learner_id]);
    evutil_make_socket_nonblocking(sock);
    l->ctx->sock = sock;
    setRcvBuf(l->ctx->sock);
    if (net_ip__is_multicast_ip(conf->learner_address[learner_id])) {
        subcribe_to_multicast_group(conf->learner_address[learner_id], sock);
    }

    ip_to_sockaddr(conf->acceptor_address, conf->acceptor_port, &(l->ctx->acceptor_sin));
   
    int i;
    //l->iovecs = (iovec*) malloc (sizeof(iovec) * VLEN);
    //l->msgs = (mmsghdr*) malloc (sizeof(mmsghdr) * VLEN);
    l->iovecs = malloc (sizeof( struct iovec) * VLEN);
    l->msgs = malloc (sizeof( struct mmsghdr) * VLEN);
    memset(l->msgs, 0, sizeof(l->msgs) * VLEN);
    memset(l->iovecs, 0, sizeof(l->iovecs) * VLEN);

    for (i = 0; i < VLEN; i++) {
        l->iovecs[i].iov_base         = l->bufs[i];
        l->iovecs[i].iov_len          = BUFSIZE;
        l->msgs[i].msg_hdr.msg_iov    = &(l->iovecs[i]);
        l->msgs[i].msg_hdr.msg_iovlen = 1;
    }
    l->timeout.tv_sec = TIMEOUT;
    l->timeout.tv_nsec = 0;

    l->max_received = 0;
    
    
    time_t t;
    srand((unsigned) time(&t));

    l->ctx->ev_read = event_new(l->ctx->base, sock, EV_READ|EV_PERSIST,learner_read_cb, l);
    event_add(l->ctx->ev_read, NULL);

    l->ctx->learner_state = learner_new(conf->acceptor_count);
    learner_set_instance_id(l->ctx->learner_state, 0);

    l->ctx->deliver = f;
    l->ctx->deliver_arg = arg;

    l->ctx->tv.tv_sec = 1;
    // check holes every 1s + ~100 ms 
    l->ctx->tv.tv_usec = 100000 * (rand() % 7);
    //l->ctx->tv.tv_usec = 0;

    l->ctx->hole_watcher = event_new(l->ctx->base, sock, EV_TIMEOUT|EV_PERSIST, check_holes, l);
    event_add(l->ctx->hole_watcher, &(l->ctx->tv));

    return l;
}
