#ifndef NETPAXOS_H_
#define NETPAXOS_H_

#include <event2/event.h>
#include <sys/socket.h>
#include "paxos.h"
#include <pthread.h>
#define BUFSIZE 386
#define NUM_OF_THREAD 2
#define ALL 65

pthread_mutex_t execute_mutex;
pthread_cond_t execute;
pthread_attr_t attr;
cpu_set_t cpus;

pthread_attr_t attr;
int check_leveldb;

struct learner_thread
{
   
    uint16_t lth_id;
    struct paxos_ctx *ctx;
};

struct leveldb_ctx *common_levelb;
struct learner_thread** learners;

/**
 * When starting a learner you must pass a callback to be invoked whenever
 * a value has been learned.
 */
typedef void (*respond_callback)(char* value, size_t size, void* arg);
typedef void (*deliver_function)(int tid, unsigned int, char* value, size_t size, void* arg);

struct sequencer {
    uint32_t current_instance[NUM_OF_THREAD];
};

struct netpaxos_configuration {
    int *proposer_port;
    char **proposer_address;
    int acceptor_port;
    char *acceptor_address;
    //int learner_port;
    //char *learner_address;
    int *learner_port;
    char ** learner_address;
    int coordinator_port;
    char *coordinator_address;
    int acceptor_count;
    int proposer_count;
    int learner_count;
    int max_num_proposer;
    int max_num_learner_thread;
    int proxy_port;
};

struct paxos_ctx {
    int my_id;
    int sock;
    struct sockaddr_in coordinator_sin;
    struct sockaddr_in acceptor_sin;
    struct sockaddr_in *learner_sin;
    struct sockaddr_in proposer_sin;
    struct event_base *base;
    struct event *ev_read, *ev_send, *ev_sigint, *ev_sigterm, *hole_watcher, *timeout_ev;
    struct learner* learner_state;
    struct acceptor* acceptor_state;
    struct proposer* proposer_state;
    struct sequencer* sequencer;
    deliver_function deliver;
    void* deliver_arg;
    respond_callback respond;
    void* respond_arg;
    struct timeval tv;
    int at_second;
    int message_per_second;
    char* buffer;
};

void submit(struct paxos_ctx *ctx, char *value, int size, uint16_t thread_id);

void init_paxos_ctx(struct paxos_ctx *ctx);
struct paxos_ctx *make_proposer(struct netpaxos_configuration *conf,
    int proposer_id, void *arg);
/*struct paxos_ctx *make_learner(struct netpaxos_configuration *conf,
                                        int thread_id,
                                        deliver_function f, void *arg);*/

//void set_instance_id (struct learner* l, iid_t iid);
struct learner_thread* make_learner(int learner_id, struct netpaxos_configuration *conf, deliver_function f, void *arg);
//struct paxos_ctx* make_learner(int learner_id, struct netpaxos_configuration *conf, deliver_function f, void *arg);

struct paxos_ctx *make_replica(struct netpaxos_configuration *conf,int thread_id,
                                        deliver_function f, void *arg);
void start_paxos(struct paxos_ctx *ctx);
void handle_signal(evutil_socket_t fd, short what, void *arg);
void free_paxos_ctx(struct paxos_ctx *ctx);
void subcribe_to_multicast_group(char *group, int sockfd);
void on_perf(evutil_socket_t fd, short event, void *arg);

void check_holes(evutil_socket_t fd, short event, void *arg);
void learner_read_cb(evutil_socket_t fd, short what, void *arg);

struct paxos_ctx *make_coordinator(struct netpaxos_configuration *conf, int my_id);
struct paxos_ctx *make_acceptor(struct netpaxos_configuration *conf, int aid);
struct paxos_ctx* make_sequencer(struct netpaxos_configuration *conf);
//unsigned long hash(const char *s);
#endif