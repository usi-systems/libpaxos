#ifndef NETPAXOS_H_
#define NETPAXOS_H_

#include <event2/event.h>
#include <sys/socket.h>

#define BUFSIZE 1024

/**
 * When starting a learner you must pass a callback to be invoked whenever
 * a value has been learned.
 */
typedef void (*deliver_function)(
    unsigned int,
    char* value,
    size_t size,
    void* arg);


struct netpaxos_configuration {
    int acceptor_port;
    char *acceptor_address;
    int learner_port;
    char *learner_address;
    int acceptor_count;
};

struct paxos_ctx {
    int sock;
    struct sockaddr_in dest;
    struct event_base *base;
    struct event *ev_read, *ev_send, *ev_signal, *hole_watcher;
    struct learner *learner_state;
    deliver_function deliver;
    void* deliver_arg;
    struct timeval tv;
};

void submit(struct paxos_ctx *ctx, char *value, int size);

void init_paxos_ctx(struct paxos_ctx *ctx);
struct paxos_ctx *make_proposer(const char *host, int port);
struct paxos_ctx *make_learner(struct netpaxos_configuration *conf,
                                        deliver_function f, void *arg);
void start_paxos(struct paxos_ctx *ctx);
void handle_signal(evutil_socket_t fd, short what, void *arg);
void free_paxos_ctx(struct paxos_ctx *ctx);

#endif