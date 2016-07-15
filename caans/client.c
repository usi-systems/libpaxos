#include <stdio.h>
#include <stdlib.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include "message.h"

#define BILLION 1000000000

struct client_context {
    struct event_base *base;
    struct sockaddr_in server_addr;
    int command_id;
};


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
    printf("Caught signal\n");
    struct client_context *ctx = arg;
    event_base_loopbreak(ctx->base);
}

void send_to_addr(int fd, struct client_context *ctx) {
    socklen_t addr_size = sizeof (ctx->server_addr);
    struct command cmd;
    cmd.command_id = ctx->command_id++;
    clock_gettime(CLOCK_REALTIME, &cmd.ts);
    // memset(cmd.content, 'c', 15);
    // cmd.content[15] = '\0';

    int msg_size = sizeof cmd;   
    int n = sendto(fd , &cmd, msg_size, 0, (struct sockaddr *)&ctx->server_addr, addr_size);
    if (n < 0) {
        perror("sendto");
    }
}

void on_read(evutil_socket_t fd, short event, void *arg) {
    struct client_context *ctx = arg;
    struct sockaddr_in remote;
    socklen_t addrlen = sizeof(remote);
    struct command response;
    int n = recvfrom(fd, &response, sizeof(response), 0, (struct sockaddr*)&remote, &addrlen);
    if (n < 0)
        perror("recvfrom");
    struct timespec end;
    clock_gettime(CLOCK_REALTIME, &end);
    struct timespec result;
    if ( timespec_diff(&result, &end, &response.ts) < 1)
        printf("%ld.%09ld\n", result.tv_sec, result.tv_nsec);


    send_to_addr(fd, ctx);
}

int new_dgram_socket() {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("New DGRAM socket");
        return -1;
    }
    return s;
}


int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Syntax: %s [hostname] [port]\n"
               "Example: %s 192.168.1.110 6789\n",argv[0], argv[0]);
        return 1;
    }

    struct hostent *server;
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    struct client_context ctx;
    ctx.command_id = 0;
    memset(&ctx.server_addr, 0, sizeof ctx.server_addr);
    ctx.server_addr.sin_family = AF_INET;
    memcpy((char *)&(ctx.server_addr.sin_addr.s_addr), (char *)server->h_addr, server->h_length);
    ctx.server_addr.sin_port = htons(atoi(argv[2]));

    ctx.base = event_base_new();
    int sock = new_dgram_socket();
    evutil_make_socket_nonblocking(sock);

    struct event *ev_read, *ev_sigint, *ev_sigterm;
    ev_read = event_new(ctx.base, sock, EV_READ|EV_PERSIST, on_read, &ctx);
    event_add(ev_read, NULL);

    ev_sigint = evsignal_new(ctx.base, SIGINT, handle_signal, &ctx);
    evsignal_add(ev_sigint, NULL);

    ev_sigterm = evsignal_new(ctx.base, SIGTERM, handle_signal, &ctx);
    evsignal_add(ev_sigterm, NULL);

    send_to_addr(sock, &ctx);

    event_base_dispatch(ctx.base);

    event_free(ev_read);
    event_free(ev_sigint);
    event_free(ev_sigterm);
    event_base_free(ctx.base);

    return 0;
}