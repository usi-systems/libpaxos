#define _GNU_SOURCE
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

#define VLEN 1024
#define TIMEOUT 1

static struct mmsghdr tx_buffer[VLEN];
static struct iovec tx_iovecs[VLEN];

static struct mmsghdr msgs[VLEN];
static struct iovec iovecs[VLEN];
static char bufs[VLEN][BUFSIZE+1];
static struct timespec timeout;

static int tx_buffer_len;

static void flush_buffer(evutil_socket_t fd, short what, void *arg) {
    int retval = sendmmsg(fd, tx_buffer, tx_buffer_len, 0);
    if (retval == -1)
        perror("sendmmsg()");
    tx_buffer_len = 0;
}

void submit(struct paxos_ctx *ctx, char *value, int size, uint16_t thread_id) {
    printf("before submit, thread_id is  %d\n", thread_id);
    struct paxos_message msg = {
        .type = PAXOS_ACCEPT,
        .u.accept.iid = 1,
        .u.accept.ballot = 0,
        .u.accept.thread_id  = thread_id,
        .u.accept.value_ballot = 0,
        .u.accept.aid = 0,
        .u.accept.value.paxos_value_len = size,
        .u.accept.value.paxos_value_val = value
    };

    char buffer[BUFSIZE];
    pack_paxos_message(buffer, &msg);
    size_t msg_len = sizeof(struct paxos_message) + size;

    tx_iovecs[tx_buffer_len].iov_base = buffer;
    tx_iovecs[tx_buffer_len].iov_len = msg_len;
    tx_buffer[tx_buffer_len].msg_hdr.msg_name = &ctx->coordinator_sin;
    tx_buffer[tx_buffer_len].msg_hdr.msg_namelen = sizeof(ctx->coordinator_sin);
    tx_buffer[tx_buffer_len].msg_hdr.msg_iov = &tx_iovecs[tx_buffer_len];
    tx_buffer[tx_buffer_len].msg_hdr.msg_iovlen = 1;
    tx_buffer_len++;

    flush_buffer(ctx->sock, EV_TIMEOUT, NULL);
}

void proposer_read_cb(evutil_socket_t fd, short what, void *arg) {
    if (what&EV_READ) {
        int retval;
        retval = recvmmsg(fd, msgs, VLEN, 0, &timeout);
        if (retval == -1) {
            perror("recvmmsg()");
            exit(EXIT_FAILURE);
        }
    }
}

struct paxos_ctx *make_proposer(struct netpaxos_configuration *conf,
    int proposer_id, void *arg)
{
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
    tx_buffer_len = 0;

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
    setRcvBuf(ctx->sock);
    /* flush buffer every 100 us */
    /*
    struct timeval flush_timer = {0, 10000};
    ctx->hole_watcher = event_new(ctx->base, sock, EV_TIMEOUT|EV_PERSIST,
        flush_buffer, NULL);
    event_add(ctx->hole_watcher, &flush_timer);
    */
    ctx->ev_sigint = evsignal_new(ctx->base, SIGINT, handle_signal, ctx);
    evsignal_add(ctx->ev_sigint, NULL);

    ctx->ev_sigterm = evsignal_new(ctx->base, SIGTERM, handle_signal, ctx);
    evsignal_add(ctx->ev_sigterm, NULL);

    return ctx;
}


