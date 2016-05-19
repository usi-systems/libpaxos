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

struct paxos_ctx *make_replica(struct netpaxos_configuration *conf,
                                        deliver_function f, void *arg)
{
    struct paxos_ctx *ctx = malloc( sizeof(struct paxos_ctx));
    init_paxos_ctx(ctx);
    ctx->base = event_base_new();

    /* Learner part */
    evutil_socket_t sock = create_server_socket(conf->learner_port);
    evutil_make_socket_nonblocking(sock);
    ctx->sock = sock;
    if (net_ip__is_multicast_ip(conf->learner_address)) {
        subcribe_to_multicast_group(conf->learner_address, sock);
    }
    ip_to_sockaddr(conf->acceptor_address, conf->acceptor_port, &ctx->acceptor_sin);

    ctx->ev_read = event_new(ctx->base, sock, EV_READ|EV_PERSIST,
        learner_read_cb, ctx);
    event_add(ctx->ev_read, NULL);

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

    /* Proposer part */
    ip_to_sockaddr(conf->learner_address, conf->learner_port, &ctx->learner_sin);

    return ctx;
}
