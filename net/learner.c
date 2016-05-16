#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <signal.h>
#include "netutils.h"
#include "netpaxos.h"
#include "message_pack.h"
#include "paxos.h"

void learner_read_cb(evutil_socket_t fd, short what, void *arg) {
    if (what&EV_READ) {
        char buffer[BUFSIZE];
        memset(buffer, 0, BUFSIZE);
        struct sockaddr_in remote;
        socklen_t readlen = sizeof(remote);

        int n = recvfrom(fd, buffer, BUFSIZE, 0, (struct sockaddr *)&remote,
            &readlen);
        if (n < 0)
            perror("recvfrom");
        printf("%s\n", buffer);

        int i;
        for (i = 0; i < n; i++)
            printf("%.2x ", buffer[i]);
        printf("\n");

        struct paxos_message msg;
        unpack_paxos_message(&msg, buffer);

        if (msg.type == PAXOS_ACCEPT) {
            printf("iid %d, ballot %d, value_ballot %d, aid %d, value[%d, %s]\n",
                msg.u.accept.iid, msg.u.accept.ballot,
                msg.u.accept.value_ballot, msg.u.accept.aid,
                msg.u.accept.value.paxos_value_len,
                msg.u.accept.value.paxos_value_val);
        }
        paxos_message_destroy(&msg);
        // n = sendto(fd, buffer, n, 0, (struct sockaddr *)&remote, readlen);
        // if (n < 0)
        //     perror("sendto");
    }
    else if (what&EV_TIMEOUT) {
        printf("Event timeout\n");
    }
}


struct paxos_ctx *make_learner(int port)
{
    struct paxos_ctx *ctx = malloc( sizeof(struct paxos_ctx));
    init_paxos_ctx(ctx);
    ctx->base = event_base_new();

    evutil_socket_t sock = create_server_socket(port);
    evutil_make_socket_nonblocking(sock);

    ctx->ev_read = event_new(ctx->base, sock, EV_TIMEOUT|EV_READ|EV_PERSIST,
        learner_read_cb, ctx);
    struct timeval one_second = {5,0};
    event_add(ctx->ev_read, &one_second);

    ctx->ev_signal = evsignal_new(ctx->base, SIGINT, handle_signal, ctx);
    evsignal_add(ctx->ev_signal, NULL);

    return ctx;
}
