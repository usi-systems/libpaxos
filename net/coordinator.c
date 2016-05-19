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

void received_fast_accept(evutil_socket_t fd, short what, void *arg)
{
    struct paxos_ctx *ctx = arg;
    if (what&EV_READ) {
        char buffer[BUFSIZE];
        memset(buffer, 0, BUFSIZE);
        struct sockaddr_in remote;
        socklen_t len = sizeof(remote);

        int n = recvfrom(fd, buffer, BUFSIZE, 0, (struct sockaddr *)&remote, &len);
        if (n < 0)
            perror("recvfrom");

        struct paxos_message msg;
        unpack_paxos_message(&msg, buffer);

        if (msg.type == PAXOS_ACCEPT) {
            msg.u.accept.iid = ctx->mock_instance++;
            /* TODO: remove the line below. Acceptor should change
                PAXOS_ACCEPT to PAXOS_ACCEPTED */
            msg.type = PAXOS_ACCEPTED;

            pack_paxos_message(buffer, &msg);
            size_t msg_len = sizeof(struct paxos_message);
            n = sendto(fd, buffer, msg_len, 0,
                (struct sockaddr *)&ctx->acceptor_sin, sizeof(ctx->learner_sin));
            if (n < 0)
                perror("Sendto:");
        }
        paxos_message_destroy(&msg);
    }
}


struct paxos_ctx *make_coordinator(struct netpaxos_configuration *conf)
{
    struct paxos_ctx *ctx = malloc( sizeof(struct paxos_ctx));
    init_paxos_ctx(ctx);
    ctx->base = event_base_new();

    evutil_socket_t sock = create_server_socket(conf->coordinator_port);
    evutil_make_socket_nonblocking(sock);
    ctx->sock = sock;

    ip_to_sockaddr(conf->acceptor_address, conf->acceptor_port, &ctx->acceptor_sin);

    ctx->ev_read = event_new(ctx->base, sock, EV_READ|EV_PERSIST,
        received_fast_accept, ctx);
    event_add(ctx->ev_read, NULL);

    ctx->ev_signal = evsignal_new(ctx->base, SIGINT, handle_signal, ctx);
    evsignal_add(ctx->ev_signal, NULL);

    return ctx;
}
