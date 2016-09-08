#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <signal.h>
#include "paxos.h"

#include "netutils.h"
#include "netpaxos.h"
#include "message_pack.h"


static void
sequencer_handle_proposal(struct paxos_ctx *ctx, char* buf, int size)
{
    uint32_t* inst_be = (uint32_t *)(buf + sizeof(uint16_t));
    *inst_be = htonl(ctx->sequencer->current_instance++);
    hexdump(buf, size);
    int n = sendto(ctx->sock, buf, size, 0,
        (struct sockaddr *)&ctx->acceptor_sin, sizeof(ctx->acceptor_sin));
    if (n < 0)
        perror("Sendto:");
}

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
            perror("recvfrom");
        uint16_t* msgtype_be = (uint16_t *)buf;
        uint16_t msgtype = ntohs(*msgtype_be);
        hexdump(buf, n);
        switch(msgtype) {
            case PAXOS_ACCEPT:
                sequencer_handle_proposal(ctx, buf, n);
                break;
            default:
                paxos_log_debug("No handler for message type %d\n", msgtype);
        }
    }
}

struct sequencer*
sequencer_new() {
    struct sequencer* sq = malloc(sizeof(struct sequencer));
    sq->current_instance = 1;
    return sq;
}

struct paxos_ctx *make_sequencer(struct netpaxos_configuration *conf)
{
    struct paxos_ctx *ctx = malloc( sizeof(struct paxos_ctx));
    init_paxos_ctx(ctx);

    ctx->sequencer = sequencer_new();

    ctx->base = event_base_new();

    evutil_socket_t sock = create_server_socket(conf->coordinator_port);
    evutil_make_socket_nonblocking(sock);
    ctx->sock = sock;

    ip_to_sockaddr(conf->acceptor_address, conf->acceptor_port,
                    &ctx->acceptor_sin);

    ctx->ev_read = event_new(ctx->base, sock, EV_READ|EV_PERSIST,
        handle_packet_in, ctx);
    event_add(ctx->ev_read, NULL);

    ctx->ev_sigint = evsignal_new(ctx->base, SIGINT, handle_signal, ctx);
    evsignal_add(ctx->ev_sigint, NULL);

    ctx->ev_sigterm = evsignal_new(ctx->base, SIGTERM, handle_signal, ctx);
    evsignal_add(ctx->ev_sigterm, NULL);

    return ctx;
}
