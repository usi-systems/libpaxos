#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <signal.h>
#include <errno.h>
#include <error.h>

#include "paxos.h"
#include "netutils.h"
#include "netpaxos.h"
#include "message_pack.h"


static void
sequencer_handle_proposal(struct paxos_ctx *ctx, char* buf, int size)
{
    uint32_t* inst_be = (uint32_t *)(buf + sizeof(uint16_t));
    *inst_be = htonl(ctx->sequencer->current_instance++);
    // hexdump(buf, size);
    int n = sendto(ctx->sock, buf, size, 0,
        (struct sockaddr *)&ctx->acceptor_sin, sizeof(ctx->acceptor_sin));
    if (n < 0)
        error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
}


static void
handle_accepted(struct paxos_ctx *ctx, char* buf, int size, struct sockaddr_in *remote, size_t socklen)
{
    struct paxos_message msg;
    unpack_paxos_message(&msg, buf);
    char *val = (char*)msg.u.accepted.value.paxos_value_val;
    /* Skip command ID and client address */
    char *retval = (val + sizeof(uint16_t) + sizeof(struct sockaddr_in));
    size_t retsize = size - sizeof(msg.u.accepted) + sizeof(uint16_t) + socklen;
    int n = sendto(ctx->sock, retval, retsize, 0,
        (struct sockaddr *)remote, socklen);
    ctx->message_per_second++;
    if (n < 0)
        error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
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
            error_at_line(1, errno, __FILE__, __LINE__, "%s\n", strerror(errno));
        uint16_t* msgtype_be = (uint16_t *)buf;
        uint16_t msgtype = ntohs(*msgtype_be);
        // hexdump(buf, n);
        switch(msgtype) {
            case PAXOS_ACCEPT:
                sequencer_handle_proposal(ctx, buf, n);
                break;
            case PAXOS_ACCEPTED: // use ACCEPTED for benchmarking
                handle_accepted(ctx, buf, n, &remote, len);
                break;
            default:
                break;
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

    if (net_ip__is_multicast_ip(conf->coordinator_address)) {
        subcribe_to_multicast_group(conf->coordinator_address, sock);
    }

    ip_to_sockaddr(conf->acceptor_address, conf->acceptor_port,
                    &ctx->acceptor_sin);

    ctx->ev_read = event_new(ctx->base, sock, EV_READ|EV_PERSIST,
        handle_packet_in, ctx);
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
