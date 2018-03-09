#include <stdio.h>
#include <stdlib.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <rte_byteorder.h>
#include <rte_memcpy.h>

#include "app_hdr.h"
#include "paxos.h"
#include "main.h"
#include "dpp_paxos.h"

#define BILLION 1000000000

struct client_context {
    struct event_base *base;
    struct sockaddr_in server_addr;
    uint32_t cur_inst;
};


static void
init_paxos_hdr(struct paxos_hdr *px, uint32_t inst, char* value, int size) {
	px->msgtype = rte_cpu_to_be_16(PAXOS_ACCEPTED);
	px->inst = rte_cpu_to_be_32(inst);
	px->rnd = rte_cpu_to_be_16(0);
	px->vrnd = rte_cpu_to_be_16(0);
	px->acptid = rte_cpu_to_be_16(0);
	px->value_len = rte_cpu_to_be_32(size);
    rte_memcpy(&px->value, value, size);
	px->igress_ts = rte_cpu_to_be_64(0);
	px->egress_ts = rte_cpu_to_be_64(0);
}

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
    struct sockaddr_in remote;
    socklen_t addrlen = sizeof(remote);
    struct paxos_hdr msg;
    int n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr*)&remote, &addrlen);
    if (n < 0)
        perror("recvfrom");
    struct timespec end;
    clock_gettime(CLOCK_REALTIME, &end);
    struct timespec result;
    struct app_hdr *ap = (struct app_hdr*)&msg.value;
    if ( timespec_diff(&result, &end, &ap->start) < 1)
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

static uint8_t DEFAULT_KEY[] = "ABCDEFGH1234567";
static uint8_t DEFAULT_VALUE[] = "ABCDEFGH1234567";

void send_to_addr(int fd, struct client_context *ctx) {
    struct app_hdr msg;
    msg.msg_type = WRITE_OP;
    msg.key_len = rte_cpu_to_be_32(sizeof(DEFAULT_KEY));
    msg.value_len = rte_cpu_to_be_32(sizeof(DEFAULT_VALUE));
    rte_memcpy(msg.key, DEFAULT_KEY, sizeof(DEFAULT_KEY));
    if (msg.msg_type == WRITE_OP) {
		rte_memcpy(msg.value, DEFAULT_VALUE, sizeof(DEFAULT_VALUE));
	}
    clock_gettime(CLOCK_REALTIME, &msg.start);
    struct paxos_hdr px;
    init_paxos_hdr(&px, ctx->cur_inst++, (char*)&msg, sizeof (struct app_hdr));
    int msg_size = sizeof (struct paxos_hdr);

    socklen_t addr_size = sizeof (ctx->server_addr);
    int n = sendto(fd , &px, msg_size, 0, (struct sockaddr *)&ctx->server_addr, addr_size);
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
    ctx.cur_inst = 1;

    memset(&ctx.server_addr, 0, sizeof ctx.server_addr);
    ctx.server_addr.sin_family = AF_INET;
    memcpy((char *)&(ctx.server_addr.sin_addr.s_addr), (char *)server->h_addr, server->h_length);
    ctx.server_addr.sin_port = htons(12345);

    // printf("address %s, port %d\n", inet_ntoa(ctx.server_addr.sin_addr),ntohs(ctx.server_addr.sin_port));

    ctx.base = event_base_new();
    int sock = new_dgram_socket();
    evutil_make_socket_nonblocking(sock);

    struct event *ev_read, *ev_signal;
    ev_read = event_new(ctx.base, sock, EV_READ|EV_PERSIST, on_read, &ctx);
    event_add(ev_read, NULL);

    ev_signal = evsignal_new(ctx.base, SIGINT|SIGTERM, handle_signal, &ctx);
    evsignal_add(ev_signal, NULL);

    int i;
    for (i=0; i< 150; i++)
        send_to_addr(sock, &ctx);

    event_base_dispatch(ctx.base);

    event_free(ev_read);
    event_free(ev_signal);
    event_base_free(ctx.base);

    return 0;
}
