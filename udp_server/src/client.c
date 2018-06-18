#include <stdio.h>
#include <stdlib.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#include "paxos.h"

#define BILLION 1000000000

struct client_context {
    struct event_base *base;
    struct sockaddr_in server_addr;
};

void send_to_addr(int fd, struct client_context *ctx);

int
timespec_diff(struct timespec *result, struct timespec *end,struct timespec *start)
{
    if (end->tv_nsec < start->tv_nsec) {
        result->tv_nsec =  BILLION + end->tv_nsec - start->tv_nsec;
        result->tv_sec = end->tv_sec - start->tv_sec - 1;
    } else {
        result->tv_nsec = end->tv_nsec - start->tv_nsec;
        result->tv_sec = end->tv_sec - start->tv_sec;
    }
  /* Return 1 if result is negative. */
  return end->tv_sec < start->tv_sec;
}


void handle_signal(evutil_socket_t fd, short what, void *arg)
{
    struct client_context *ctx = arg;
    event_base_loopbreak(ctx->base);
}



void on_read(evutil_socket_t fd, short event, void *arg) {
    struct client_context *ctx = arg;
    if (event & EV_READ) {
        struct sockaddr_in remote;
        socklen_t addrlen = sizeof(remote);
        struct paxos_hdr msg;
        int n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr*)&remote, &addrlen);
        if (n < 0)
        perror("recvfrom");
        deserialize_paxos_hdr(&msg);
        // print_paxos_hdr(&msg);
        if (msg.igress_ts.tv_sec > 0) {
            struct timespec end;
            clock_gettime(CLOCK_REALTIME, &end);
            struct timespec result;
            if ( timespec_diff(&result, &end, &msg.igress_ts) < 1)
            printf("%ld.%09ld\n", result.tv_sec, result.tv_nsec);
            msg.igress_ts = end;
        }
        msg.msgtype = NEW_COMMAND;
        if (msg.inst % 1024 == 0)
        {
            clock_gettime(CLOCK_REALTIME, &msg.igress_ts);
        } else {
            msg.igress_ts.tv_sec = 0;
            msg.igress_ts.tv_nsec = 0;
        }
        serialize_paxos_hdr(&msg);
        n = sendto(fd , &msg, sizeof(msg), 0, (struct sockaddr *)&remote, addrlen);
        if (n < 0)
        perror("Send back");
    }
    else if (event & EV_TIMEOUT) {
        send_to_addr(fd, ctx);
    }
}

int new_dgram_socket() {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("New DGRAM socket");
        return -1;
    }
    return s;
}

void send_to_addr(int fd, struct client_context *ctx) {
    socklen_t addr_size = sizeof (ctx->server_addr);
    struct paxos_hdr msg;
    // clock_gettime(CLOCK_REALTIME, &msg.igress_ts);
    msg.msgtype = NEW_COMMAND;
    msg.worker_id = 0;
    msg.rnd = 0;
    msg.inst =0;
    msg.log_index = 0;
    msg.vrnd = 0;
    msg.acptid = 0;
    msg.igress_ts.tv_sec = 0;
    msg.igress_ts.tv_nsec = 0;
    int msg_size = sizeof msg;
    int n = sendto(fd , &msg, msg_size, 0, (struct sockaddr *)&ctx->server_addr, addr_size);
    if (n < 0) {
        perror("sendto");
    }
}


int main(int argc, char *argv[])
{
    struct hostent *server;
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    struct client_context ctx;
    memset(&ctx.server_addr, 0, sizeof ctx.server_addr);
    ctx.server_addr.sin_family = AF_INET;
    memcpy((char *)&(ctx.server_addr.sin_addr.s_addr), (char *)server->h_addr, server->h_length);
    ctx.server_addr.sin_port = htons(9081);

    // printf("address %s, port %d\n", inet_ntoa(ctx.server_addr.sin_addr),ntohs(ctx.server_addr.sin_port));

    ctx.base = event_base_new();
    int sock = new_dgram_socket();
    evutil_make_socket_nonblocking(sock);

    struct event *ev_read, *ev_signal;

    struct timeval one_second = {1, 0};
    ev_read = event_new(ctx.base, sock, EV_READ|EV_PERSIST|EV_TIMEOUT, on_read, &ctx);
    event_add(ev_read, &one_second);

    ev_signal = evsignal_new(ctx.base, SIGINT|SIGTERM, handle_signal, &ctx);
    evsignal_add(ev_signal, NULL);

    send_to_addr(sock, &ctx);

    event_base_dispatch(ctx.base);

    event_free(ev_read);
    event_free(ev_signal);
    event_base_free(ctx.base);

    return 0;
}
