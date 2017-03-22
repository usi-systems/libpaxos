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

#define MAX_GAPS 10

#define VLEN 1024
#define TIMEOUT 1
int counter = 0;
static struct mmsghdr msgs[VLEN];
static struct iovec iovecs[VLEN];
static char bufs[VLEN][BUFSIZE+1];
static struct timespec timeout;

static uint32_t max_received;

//static struct mmsghdr tx_buffer[VLEN];
//static struct iovec tx_iovecs[VLEN];


static void
send_paxos_message(struct learner_thread *l, paxos_message* msg, int msg_size) {
    char buffer[BUFSIZE];
    //int tid = l->lth_id;
    pack_paxos_message(buffer, msg);
    sendto(l->ctx->sock, buffer, msg_size, 0,
            (struct sockaddr*)&(l->ctx->acceptor_sin), sizeof(l->ctx->acceptor_sin));
    // print_addr(&ctx->acceptor_sin);
}

void check_holes(evutil_socket_t fd, short event, void *arg) {
    struct learner_thread *l = (struct learner_thread *)arg;
    unsigned from_inst;
    unsigned to_inst;
    //int index = 0;

    if (learner_has_holes(l->ctx->learner_state, &from_inst, &to_inst)) {
        paxos_log_debug("Learner thread %u has holes from %d to %d", l->lth_id, from_inst, to_inst);
        //if ((to_inst - from_inst) > MAX_GAPS)
        //    to_inst = from_inst + MAX_GAPS;
        int iid;

        /*for (iid = from_inst; iid < to_inst; iid++) {
            paxos_message out;
            out.type = PAXOS_PREPARE;
            learner_prepare(l->ctx->learner_state, &out.u.prepare, iid, l->lth_id);
            // send_paxos_message(ctx, &out, sizeof(paxos_message));
            paxos_log_debug("send prepare message with iid %u thread_id %u a_tid %u\n", 
                out.u.prepare.iid, out.u.prepare.thread_id, out.u.prepare.a_tid);
            char buffer[BUFSIZE];
            pack_paxos_message(buffer, &out);
            int idx = iid - from_inst;
            tx_iovecs[idx].iov_base = buffer;
            tx_iovecs[idx].iov_len = sizeof(paxos_message);
            tx_buffer[idx].msg_hdr.msg_name = &(l->ctx->acceptor_sin);
            tx_buffer[idx].msg_hdr.msg_namelen = sizeof(l->ctx->acceptor_sin);
            tx_buffer[idx].msg_hdr.msg_iov = &tx_iovecs[idx];
            tx_buffer[idx].msg_hdr.msg_iovlen = 1;
        }*/
        iid = from_inst;
        paxos_message out;
        out.type = PAXOS_PREPARE;
        learner_prepare(l->ctx->learner_state, &out.u.prepare, iid, l->lth_id);
            
        paxos_log_debug("send prepare message with iid %u thread_id %u a_tid %u\n", 
        out.u.prepare.iid, out.u.prepare.thread_id, out.u.prepare.a_tid);
        //char buffer[BUFSIZE];
        //pack_paxos_message(buffer, &out);
        send_paxos_message(l, &out, sizeof(paxos_message));  
        /*tx_iovecs[index].iov_base = buffer;
        tx_iovecs[index].iov_len = sizeof(paxos_message);
        tx_buffer[index].msg_hdr.msg_name = &(l->ctx->acceptor_sin);
        tx_buffer[index].msg_hdr.msg_namelen = sizeof(l->ctx->acceptor_sin);
        tx_buffer[index].msg_hdr.msg_iov = &tx_iovecs[index];
        tx_buffer[index].msg_hdr.msg_iovlen = 1;
        unsigned vlen = 1;
        int retval = sendmmsg(l->ctx->sock, tx_buffer, vlen, 0);
        if (retval == -1)
            perror("sendmmsg()");*/
    }
}

void on_paxos_accepted(paxos_message *msg, struct learner_thread *l_th) {

    uint16_t tid = l_th->lth_id;
    learner_receive_accepted(l_th->ctx->learner_state, &msg->u.accepted, tid);

    paxos_accepted chosen_value;
    
    //printf("thread %d\n", tid);
   
    while (learner_deliver_next(l_th->ctx->learner_state, &chosen_value, tid)) {
       
        if (chosen_value.thread_id == ALL)
        {
            pthread_mutex_lock (&execute_mutex);
            counter ++;
            
            if (counter == NUM_OF_THREAD)
            {
                paxos_log_debug("thread %u reached\n", tid);
                paxos_log_debug("-1 execute iid %u of thread %u\n", chosen_value.iid, chosen_value.thread_id );
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

                paxos_log_debug("thread %d wait\n",tid);
                pthread_cond_wait(&execute, &execute_mutex);
        
            }
            pthread_mutex_unlock(&execute_mutex);
        }
        else{
            paxos_log_debug("-2 execute iid %u of thread %u\n", chosen_value.iid, chosen_value.thread_id);
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
        retval = recvmmsg(fd, msgs, VLEN, 0, &timeout);
        if (retval == -1) {
            perror("recvmmsg()");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < retval; i++) {
            bufs[i][msgs[i].msg_len] = 0;
            if (max_received < retval) {
                max_received = retval;
                // printf("max_received %d\n", max_received);
            }
            struct paxos_message msg;
            unpack_paxos_message(&msg, bufs[i]);

            switch(msg.type) {
                case PAXOS_ACCEPTED:
                    on_paxos_accepted(&msg, l);
                    break;
                case PAXOS_PROMISE:
                    on_paxos_promise(&msg, l);
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
    //l->late_start = !paxos_config.learner_catch_up;
    //l->acceptors = conf->acceptor_count;
    //printf("I am here . Thread learner %d\n",learner_id);
    evutil_socket_t sock = create_server_socket(conf->learner_port[learner_id]);
    evutil_make_socket_nonblocking(sock);
    l->ctx->sock = sock;
   // printf("Thread learner %d socket %d\n", learner_id, sock);
    setRcvBuf(l->ctx->sock);
    if (net_ip__is_multicast_ip(conf->learner_address[learner_id])) {
        subcribe_to_multicast_group(conf->learner_address[learner_id], sock);
    }

    ip_to_sockaddr(conf->acceptor_address, conf->acceptor_port, &(l->ctx->acceptor_sin));
    int i;
    memset(msgs, 0, sizeof(msgs));
    for (i = 0; i < VLEN; i++) {
        iovecs[i].iov_base         = bufs[i];
        iovecs[i].iov_len          = BUFSIZE;
        msgs[i].msg_hdr.msg_iov    = &iovecs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }
    timeout.tv_sec = TIMEOUT;
    timeout.tv_nsec = 0;

    max_received = 0;
    
    
    time_t t;
    srand((unsigned) time(&t));

    l->ctx->ev_read = event_new(l->ctx->base, sock, EV_READ|EV_PERSIST,learner_read_cb, l);
    event_add(l->ctx->ev_read, NULL);

    l->ctx->learner_state = learner_new(conf->acceptor_count);
    learner_set_instance_id(l->ctx->learner_state, 0);

    l->ctx->deliver = f;
    l->ctx->deliver_arg = arg;

    l->ctx->tv.tv_sec = 1;
    /* check holes every 1s + ~100 ms */
    l->ctx->tv.tv_usec = 100000 * (rand() % 7);

    l->ctx->hole_watcher = event_new(l->ctx->base, sock, EV_TIMEOUT|EV_PERSIST, check_holes, l);
    event_add(l->ctx->hole_watcher, &(l->ctx->tv));

    return l;
}
