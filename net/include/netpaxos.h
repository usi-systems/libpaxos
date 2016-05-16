#ifndef NETPAXOS_H_
#define NETPAXOS_H_

#include <event2/event.h>
#include <sys/socket.h>

#define BUFSIZE 1024

struct paxos_ctx {
    int sock;
    struct sockaddr_in dest;
    struct event_base *base;
    struct event *ev_read, *ev_send, *ev_signal;
};

void submit(struct paxos_ctx *ctx, char *value, int size);

void init_paxos_ctx(struct paxos_ctx *ctx);
struct paxos_ctx *make_proposer(const char *host, int port);
struct paxos_ctx *make_learner(int port);
void start_paxos(struct paxos_ctx *ctx);
void handle_signal(evutil_socket_t fd, short what, void *arg);
void free_paxos_ctx(struct paxos_ctx *ctx);

#endif