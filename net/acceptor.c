#include "acceptor.h"
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


void acceptor_handle_prepare(struct paxos_ctx *ctx, struct paxos_message *msg,
        struct sockaddr_in *remote, socklen_t socklen)
{
    paxos_message out;
    paxos_prepare* prepare = &msg->u.prepare;

    printf("Received prepare for instance %d, round %d\n",
        msg->u.prepare.iid,
        msg->u.prepare.ballot);

    if (acceptor_receive_prepare(ctx->acceptor_state, prepare, &out) != 0) {
        printf("Respond %d (Promise) for instance %d\n", out.type, out.u.promise.iid);
        pack_paxos_message(ctx->buffer, &out);
        size_t msg_len = sizeof(struct paxos_message);
        int n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
            (struct sockaddr *)remote, socklen);
        if (n < 0)
            perror("Sendto:");
        paxos_message_destroy(&out);
    }
}

void acceptor_handle_accept(struct paxos_ctx *ctx, struct paxos_message *msg,
    struct sockaddr_in *remote, socklen_t socklen)
{
    paxos_message out;
    paxos_accept* accept = &msg->u.accept;

    if (acceptor_receive_accept(ctx->acceptor_state, accept, &out) != 0) {
        if (out.type == PAXOS_ACCEPTED) {
            pack_paxos_message(ctx->buffer, &out);
            size_t msg_len = sizeof(struct paxos_message);
            int n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
                (struct sockaddr *)&ctx->learner_sin, sizeof(ctx->learner_sin));
            if (n < 0)
                perror("Sendto:");
            n = sendto(ctx->sock, ctx->buffer, msg_len, 0,
                (struct sockaddr *)remote, socklen);
            if (n < 0)
                perror("Sendto:");
        }
        paxos_message_destroy(&out);
    }
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

    ip_to_sockaddr(conf->learner_address, conf->learner_port, &ctx->learner_sin);
    ip_to_sockaddr( conf->coordinator_address, conf->coordinator_port,
                    &ctx->coordinator_sin );

    ctx->ev_read = event_new(ctx->base, sock, EV_READ|EV_PERSIST,
        acceptor_read, ctx);
    event_add(ctx->ev_read, NULL);

    ctx->ev_signal = evsignal_new(ctx->base, SIGINT, handle_signal, ctx);
    evsignal_add(ctx->ev_signal, NULL);

    return ctx;
}
