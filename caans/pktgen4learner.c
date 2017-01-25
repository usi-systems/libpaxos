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

#include "paxos_types.h"
#include "netpaxos.h"
#include "message_pack.h"

#define NS_PER_S 1000000000

struct client_context {
    struct event_base *base;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    enum Operation op;
    uint16_t command_id;
    int cur_inst;
    int sock;
    int osd;
    double latency;
    int nb_messages;
};

int
timespec_diff(struct timespec *result, struct timespec *end,struct timespec *start)
{
    if (end->tv_nsec < start->tv_nsec) {
        result->tv_nsec =  NS_PER_S + end->tv_nsec - start->tv_nsec;
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

void send_to_addr(struct client_context *ctx) {
    socklen_t addr_size = sizeof (ctx->server_addr);
    struct command cmd;
    cmd.command_id = ctx->command_id++;
    cmd.op = ctx->op;
#ifdef GET_LATENCY
    clock_gettime(CLOCK_REALTIME, &cmd.ts);
    memset(cmd.content, 'k', 15);
    cmd.content[15] = '\0';
    memset(cmd.content+16, 'v', 15);
    cmd.content[31] = '\0';
#endif
    struct client_request *req = create_client_request((char*)&cmd, sizeof(cmd));
    req->cliaddr = ctx->client_addr;

    struct paxos_message msg = {
        .type = PAXOS_ACCEPTED, // use PAXOS_ACCEPTED for benchmarking
        .u.accept.iid = ctx->cur_inst++,
        .u.accept.ballot = 0,
        .u.accept.value_ballot = 0,
        .u.accept.aid = 0,
        .u.accept.value.paxos_value_len = message_length(req),
        .u.accept.value.paxos_value_val =  (char*)req
    };

    char buffer[BUFSIZE];
    pack_paxos_message(buffer, &msg);
    size_t msg_len = sizeof(struct paxos_message) + sizeof(cmd);

    int n = sendto(ctx->sock , buffer, msg_len, 0, (struct sockaddr *)&ctx->server_addr, addr_size);
    if (n < 0) {
        perror("sendto");
    }
}

void on_perf(evutil_socket_t fd, short event, void *arg) {
    struct client_context *ctx = arg;
    if (ctx->nb_messages > 0) {
        double average_latency = ctx->latency / ctx->nb_messages / NS_PER_S;
        printf("%.09f\n", average_latency);
    }
    ctx->latency = 0.0;
    ctx->nb_messages = 0;
}

void on_read(evutil_socket_t fd, short event, void *arg) {
    struct client_context *ctx = arg;
    if (event&EV_READ) {
        struct sockaddr_in remote;
        socklen_t addrlen = sizeof(remote);
        struct command response;
        int n = recvfrom(fd, &response, sizeof(response), 0, (struct sockaddr*)&remote, &addrlen);
        if (n < 0)
            perror("recvfrom");
#ifdef GET_LATENCY
        struct timespec end;
        clock_gettime(CLOCK_REALTIME, &end);
        struct timespec result;
        if ( timespec_diff(&result, &end, &response.ts) < 1) {
    #ifdef AGGREGATE
        ctx->latency += result.tv_sec*NS_PER_S + result.tv_nsec;
        ctx->nb_messages++;
    #else
        printf("%ld.%09ld\n", result.tv_sec, result.tv_nsec);
    #endif
        }
#endif
    }
    send_to_addr(ctx);
}


int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Syntax: %s [hostname] [port] [GET/SET] [Outstanding]\n"
               "Example: %s 192.168.1.110 6789 GET 1\n",argv[0], argv[0]);
        return 1;
    }

    struct hostent *server;
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    struct client_context ctx;
    ctx.latency = 0.0;
    ctx.nb_messages = 0;
    ctx.osd = 1;

    ctx.op = GET;
    if (argc > 3) {
        if (strcmp(argv[3], "SET") == 0) {
            ctx.op = SET;
        }
    }

    if (argc > 4) {
            ctx.osd = atoi(argv[4]);
    }
    ctx.command_id = 0;
    ctx.cur_inst = 1;
    memset(&ctx.server_addr, 0, sizeof(ctx.server_addr));
    ctx.server_addr.sin_family = AF_INET;
    memcpy((char *)&(ctx.server_addr.sin_addr.s_addr), (char *)server->h_addr, server->h_length);
    ctx.server_addr.sin_port = htons(atoi(argv[2]));


    memset(&ctx.client_addr, 0, sizeof(ctx.client_addr));
    ctx.client_addr.sin_family = AF_INET;

    socklen_t slen;
    int sock;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    connect(sock, (struct sockaddr *)&ctx.server_addr, sizeof(ctx.server_addr));
    /* now retrieve the address as before */
    slen = sizeof(ctx.client_addr);
    getsockname(sock, (struct sockaddr *)&ctx.client_addr, &slen);


    ctx.base = event_base_new();
    evutil_make_socket_nonblocking(sock);
    ctx.sock = sock;
    struct event *ev_read, *ev_sigint, *ev_sigterm;

    struct timeval one_second = {1, 0};
    ev_read = event_new(ctx.base, sock, EV_READ|EV_PERSIST|EV_TIMEOUT, on_read, &ctx);

    event_add(ev_read, &one_second);

    ev_sigint = evsignal_new(ctx.base, SIGINT, handle_signal, &ctx);
    evsignal_add(ev_sigint, NULL);

    ev_sigterm = evsignal_new(ctx.base, SIGTERM, handle_signal, &ctx);
    evsignal_add(ev_sigterm, NULL);
#ifdef AGGREGATE
    struct timeval hundred_ms = {1, 0};
    struct event *ev_perf = event_new(ctx.base, -1, EV_TIMEOUT|EV_PERSIST, on_perf, &ctx);
    event_add(ev_perf, &hundred_ms);
#endif
    int i;
    for (i = 0; i < ctx.osd; i++) {
        send_to_addr(&ctx);
    }

    event_base_dispatch(ctx.base);

    event_free(ev_read);
    event_free(ev_sigint);
    event_free(ev_sigterm);
#ifdef AGGREGATE
    event_free(ev_perf);
#endif
    event_base_free(ctx.base);

    return 0;
}