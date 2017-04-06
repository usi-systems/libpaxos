#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "application_proxy.h"
#include "netutils.h"
#include "message.h"

#define VLEN 1024
#define TIMEOUT 1
#define REQ_SIZE 64
static struct mmsghdr msgs[VLEN];
static struct iovec iovecs[VLEN];
static char bufs[VLEN][REQ_SIZE+1];
static struct sockaddr_in remotes[VLEN];
static struct timespec timeout;

void handle_request(evutil_socket_t fd, short event, void *arg) {

    struct application_ctx *app = arg;
    int i, retval;
    retval = recvmmsg(fd, msgs, VLEN, 0, &timeout);
    if (retval == -1) {
        perror("recvmmsg()");
        exit(EXIT_FAILURE);
    }
    
    for (i = 0; i < retval; i++) {
        uint16_t t_id, cid;
        uint32_t cmd_id;
        int recv_bytes = iovecs[i].iov_len;
        //printf("----------\n");
        //printf("recv_bytes(data_size) %d\n", recv_bytes);
        struct client_request *req = create_client_request(bufs[i], recv_bytes, &cid, &t_id, &cmd_id);
        //printf("handle_request client_id_%u thread_id_%u cmd_id %u\n", cid, t_id, cmd_id);
        req->cliaddr = remotes[i];
        // hexdump((char*)req, req->length);
        submit(app->paxos, (char*)req, message_length(req), t_id);
        //printf("message_length %d\n", message_length(req));
        //printf("sockaddr_in %ld\n", sizeof(req->cliaddr)); 16
        app->current_request_id++;
    }
}


void start_proxy(struct application_ctx *ctx, int proxy_port) {
    int sock = create_server_socket(proxy_port);
    evutil_make_socket_nonblocking(sock);
    int i;

    memset(msgs, 0, sizeof(msgs));
    memset(remotes, 0, sizeof(remotes));
    size_t sa_len = sizeof(struct sockaddr_in);
    for (i = 0; i < VLEN; i++) {
        iovecs[i].iov_base          = bufs[i];
        iovecs[i].iov_len           = REQ_SIZE;
        msgs[i].msg_hdr.msg_iov     = &iovecs[i];
        msgs[i].msg_hdr.msg_iovlen  = 1;
        msgs[i].msg_hdr.msg_name    = &remotes[i];
        msgs[i].msg_hdr.msg_namelen = sa_len;
    }
    timeout.tv_sec = TIMEOUT;
    timeout.tv_nsec = 0;

    ctx->ev_read = event_new(ctx->paxos->base, sock, EV_READ|EV_PERSIST,
                    handle_request, ctx);
    event_add(ctx->ev_read, NULL);
}
