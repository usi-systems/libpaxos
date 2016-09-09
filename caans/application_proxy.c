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

void handle_request(evutil_socket_t fd, short event, void *arg) {
    struct application_ctx *app = arg;
    struct sockaddr_in remote;
    socklen_t addrlen = sizeof(remote);
    int recv_bytes = recvfrom(fd, app->buffer, sizeof(app->buffer), 0,
        (struct sockaddr*)&remote, &addrlen);
    if (recv_bytes < 0) {
        perror("recvfrom");
        return;
    }
    // struct timespec *ts = (struct timespec*)app->buffer;
    // printf("%ld.%09ld\n", ts->tv_sec, ts->tv_nsec);

    struct client_request *req = create_client_request(app->buffer, recv_bytes);
    req->cliaddr = remote;
    /*
    printf("address %s, port %d\n", inet_ntoa(req->cliaddr.sin_addr),
        ntohs(req->cliaddr.sin_port));
    */
    // hexdump((char*)req, req->length);

    submit(app->paxos, (char*)req, message_length(req));
    app->current_request_id++;

}


void start_proxy(struct application_ctx *ctx, int proxy_port) {
    int sock = create_server_socket(proxy_port);
    evutil_make_socket_nonblocking(sock);

    ctx->ev_read = event_new(ctx->paxos->base, sock, EV_READ|EV_PERSIST,
                    handle_request, ctx);
    event_add(ctx->ev_read, NULL);
}