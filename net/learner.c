#include "learner.h"
#include "paxos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <signal.h>
#include "netutils.h"
#include "netpaxos.h"
#include "message_pack.h"

void check_holes(evutil_socket_t fd, short event, void *arg) {
    struct paxos_ctx *ctx = arg;
    paxos_repeat msg;
    int chunks = 10;
    if (learner_has_holes(ctx->learner_state, &msg.from, &msg.to)) {
        if ((msg.to - msg.from) > chunks)
            msg.to = msg.from + chunks;
        // printf("Learner has holes from %d to %d\n", msg.from, msg.to);
        char buffer[BUFSIZE];
        paxos_message repeat = {
            .type = PAXOS_REPEAT,
            .u.repeat = msg
        };
        pack_paxos_message(buffer, &repeat);
        sendto(ctx->sock, buffer, sizeof(repeat), 0,
                    (struct sockaddr*)&ctx->dest, sizeof(ctx->dest));
    }
    event_add(ctx->hole_watcher, &ctx->tv);
}

void on_paxos_accepted(paxos_message *msg, struct paxos_ctx *ctx) {
    learner_receive_accepted(ctx->learner_state, &msg->u.accepted);
    paxos_accepted chosen_value;
    while (learner_deliver_next(ctx->learner_state, &chosen_value)) {
        ctx->deliver(
            chosen_value.iid,
            chosen_value.value.paxos_value_val,
            chosen_value.value.paxos_value_len,
            ctx->deliver_arg);
        paxos_accepted_destroy(&chosen_value);
    }

}

void learner_read_cb(evutil_socket_t fd, short what, void *arg) {
    struct paxos_ctx *ctx = arg;
    if (what&EV_READ) {
        char buffer[BUFSIZE];
        memset(buffer, 0, BUFSIZE);
        struct sockaddr_in remote;
        socklen_t readlen = sizeof(remote);

        int n = recvfrom(fd, buffer, BUFSIZE, 0, (struct sockaddr *)&remote,
            &readlen);
        if (n < 0)
            perror("recvfrom");
/*
        int i;
        for (i = 0; i < n; i++)
            printf("%.2x ", buffer[i]);
        printf("\n");
*/
        struct paxos_message msg;
        unpack_paxos_message(&msg, buffer);

        if (msg.type == PAXOS_ACCEPTED) {
            /*
            printf("iid %d, ballot %d, value_ballot %d, aid %d, value[%d, %s]\n",
                msg.u.accepted.iid, msg.u.accepted.ballot,
                msg.u.accepted.value_ballot, msg.u.accept.aid,
                msg.u.accepted.value.paxos_value_len,
                msg.u.accepted.value.paxos_value_val);
            */
            on_paxos_accepted(&msg, ctx);
        }
        paxos_message_destroy(&msg);
    }
    else if (what&EV_TIMEOUT) {
        // printf("Event timeout\n");
    }
}


struct paxos_ctx *make_learner(struct netpaxos_configuration *conf,
                                        deliver_function f, void *arg)
{
    struct paxos_ctx *ctx = malloc( sizeof(struct paxos_ctx));
    init_paxos_ctx(ctx);
    ctx->base = event_base_new();

    evutil_socket_t sock = create_server_socket(conf->learner_port);
    evutil_make_socket_nonblocking(sock);

    subcribe_to_multicast_group(conf->learner_address, sock);

    ip_to_sockaddr(conf->acceptor_address, conf->acceptor_port, &ctx->dest);

    ctx->ev_read = event_new(ctx->base, sock, EV_TIMEOUT|EV_READ|EV_PERSIST,
        learner_read_cb, ctx);
    struct timeval one_second = {5,0};
    event_add(ctx->ev_read, &one_second);

    ctx->ev_signal = evsignal_new(ctx->base, SIGINT, handle_signal, ctx);
    evsignal_add(ctx->ev_signal, NULL);

    ctx->learner_state = learner_new(conf->acceptor_count);
    learner_set_instance_id(ctx->learner_state, 0);
    ctx->deliver = f;
    ctx->deliver_arg = arg;

    ctx->tv.tv_sec = 0;
    ctx->tv.tv_usec = 100000;

    ctx->hole_watcher = evtimer_new(ctx->base, check_holes, ctx);
    event_add(ctx->hole_watcher, &ctx->tv);

    return ctx;
}
