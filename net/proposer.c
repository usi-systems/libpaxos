#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <signal.h>
#include "netutils.h"
#include "netpaxos.h"
#include "message_pack.h"


void submit(struct paxos_ctx *ctx, char *value, int size) {
    struct paxos_message msg = {
        .type = PAXOS_ACCEPT,
        .u.accept.iid = 1,
        .u.accept.ballot = 1,
        .u.accept.value_ballot = 1,
        .u.accept.aid = 1,
        .u.accept.value.paxos_value_len = size,
        .u.accept.value.paxos_value_val = value
    };

    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    pack_paxos_message(buffer, &msg);
    size_t msg_len = sizeof(struct paxos_message) + size;

    int i;
    for (i = 0; i < msg_len; i++)
        printf("%.2x ", buffer[i]);
    printf("\n");
    int n = sendto(ctx->sock, buffer, msg_len, 0, (struct sockaddr *)&ctx->dest,
        sizeof(ctx->dest));
    if (n < 0) {
        printf("Sent %d bytes\n", n);
    }
}

void proposer_read_cb(evutil_socket_t fd, short what, void *arg) {
    if (what&EV_READ) {
        char buffer[BUFSIZE];
        memset(buffer, 0, BUFSIZE);
        struct sockaddr_in remote;
        socklen_t readlen = sizeof(remote);
        int n = recvfrom(fd, buffer, BUFSIZE, 0, (struct sockaddr *)&remote,
            &readlen);
        if (n < 0){
            perror("recvfrom");
            return;
        }
        printf("%s\n", buffer);
    }
}

struct paxos_ctx *make_proposer(const char *ip_addr, int port)
{
    struct paxos_ctx *ctx = malloc( sizeof(struct paxos_ctx));
    init_paxos_ctx(ctx);
    ctx->base = event_base_new();

    evutil_socket_t sock = new_dgram_socket();
    evutil_make_socket_nonblocking(sock);

    ip_to_sockaddr(ip_addr, port, &ctx->dest);

    ctx->sock = sock;

    ctx->ev_read = event_new(ctx->base, sock, EV_TIMEOUT|EV_READ|EV_PERSIST,
        proposer_read_cb, ctx);
    struct timeval one_second = {5,0};
    event_add(ctx->ev_read, &one_second);

    ctx->ev_signal = evsignal_new(ctx->base, SIGINT, handle_signal, ctx);
    evsignal_add(ctx->ev_signal, NULL);

    return ctx;
}


