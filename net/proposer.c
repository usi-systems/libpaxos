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
        .u.accept.ballot = 0,
        .u.accept.value_ballot = 0,
        .u.accept.aid = 0,
        .u.accept.value.paxos_value_len = size,
        .u.accept.value.paxos_value_val = value
    };

    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    pack_paxos_message(buffer, &msg);
    size_t msg_len = sizeof(struct paxos_message) + size;

    int n = sendto( ctx->sock, buffer, msg_len, 0,
                    (struct sockaddr *)&ctx->coordinator_sin,
                    sizeof(ctx->coordinator_sin) );
    if (n < 0) {
        perror("submit error");
    }
}

void proposer_read_cb(evutil_socket_t fd, short what, void *arg) {
    struct paxos_ctx *ctx = arg;
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
        ctx->respond(buffer, n, ctx->respond_arg);
    }
}

struct paxos_ctx *make_proposer(struct netpaxos_configuration *conf,
    int proposer_id, respond_callback f, void *arg)
{
    struct paxos_ctx *ctx = malloc( sizeof(struct paxos_ctx));
    init_paxos_ctx(ctx);
    ctx->base = event_base_new();

    int port = conf->proposer_port[proposer_id];
    evutil_socket_t sock = create_server_socket(port);
    evutil_make_socket_nonblocking(sock);

    ip_to_sockaddr( conf->proposer_address[proposer_id],
                    conf->proposer_port[proposer_id],
                    &ctx->proposer_sin );

    ip_to_sockaddr( conf->coordinator_address,
                    conf->coordinator_port,
                    &ctx->coordinator_sin );

    ctx->sock = sock;

    ctx->respond = f;
    ctx->respond_arg = arg;

    ctx->ev_read = event_new(ctx->base, sock, EV_TIMEOUT|EV_READ|EV_PERSIST,
        proposer_read_cb, ctx);
    struct timeval one_second = {5,0};
    event_add(ctx->ev_read, &one_second);

    ctx->ev_signal = evsignal_new(ctx->base, SIGINT|SIGTERM, handle_signal, ctx);
    evsignal_add(ctx->ev_signal, NULL);

    return ctx;
}


