#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "netpaxos.h"
#include "netutils.h"
#include "configuration.h"
#include "application_proxy.h"
#include "message.h"
#include "leveldb_context.h"

static int node_id = 0;
static int node_count = 0;
static int enable_leveldb = 0;
static int amount_of_write = 0;
static char* file_config;
struct netpaxos_configuration conf;


void on_perf(evutil_socket_t fd, short event, void *arg) {
    struct application_ctx *app = arg;
    printf("%4d %8d\n", app->at_second++, app->message_per_second);
    app->message_per_second = 0;

}
static void deliver(int tid, unsigned int inst, char* val, size_t size, void* arg) {
    struct application_ctx *app = arg;
    paxos_log_debug("thread id %d message %d second %d\n",tid, app->message_per_second, app->at_second);
    app->message_per_second++;
    if (size <= 0)
        return;
    struct client_request *req = (struct client_request*)val;

    struct command *cmd = (struct command*)(val + sizeof(struct client_request) - 1);
    
   //pthread_mutex_lock (&levelb_mutex);
    if (app->enable_leveldb)
    {
        char *key = cmd->content;
        if (cmd->op == SET) {
            char *value = cmd->content + 16;
            int res = add_entry(app->leveldb, 0, key, 16, value, 16);
            if (res) {
                fprintf(stderr, "Add entry failed.\n");
            }
            paxos_log_debug("SET(%s, %s) thread_id %d\n", key, value, tid);
        }
        else if (cmd->op == GET) {
            /* check if the value is stored */
            char *stored_value = NULL;
            size_t vsize = 0;
            int res = get_value(app->leveldb, key, 16, &stored_value, &vsize);
            if (res) {
                fprintf(stderr, "get value failed.\n");
            } 
            else {
                if (stored_value != NULL) {
                    paxos_log_debug("GET key %s with the stored value %s size %zu at thread_id %d\n", key, stored_value, vsize, tid);
                    free(stored_value);
                }
            }
        }
        else if (cmd->op == INC)
        {
            paxos_log_debug("received at thread id %d\n", tid);
            char *s_key = cmd->content + 16;
            char *f_stored_value = NULL, *s_stored_value = NULL;
            size_t f_vsize = 0, s_vsize = 0;

            int f_res = get_value(app->leveldb, key, 16, &f_stored_value, &f_vsize);
            int s_res = get_value(app->leveldb, s_key, 16, &s_stored_value, &s_vsize);

            if (f_res || s_res)
            {
                fprintf(stderr, "get value failed.\n");
            }
            else{
                if (f_stored_value != NULL && s_stored_value != NULL)
                {
                    paxos_log_debug("first key %s of first value  %s\n",key, f_stored_value);
                    paxos_log_debug("second key %s second value %s\n", s_key, s_stored_value);
                    char result_value[32];
                    memset(result_value, f_stored_value[0], 8);
                    memset(result_value+8, s_stored_value[0], 7);
                    result_value[15] = '\0';
                    paxos_log_debug("first value %s second value %s => result_value %s\n", f_stored_value,s_stored_value , result_value);
                    int r = add_entry(app->leveldb, 0, key, 16, result_value, 16);
                    if (r) {
                        fprintf(stderr, "Add entry failed.\n");
                    }
                    paxos_log_debug("INC(%s, %s) with value %s on thread_id %d\n", key, s_key, result_value, tid);
                }
            }
        }
    }
   
    /* Skip command ID and client address */
    char *retval = (val + sizeof(uint16_t) + sizeof(struct sockaddr_in));

    /* TEST only the first learner responds */
    // if (cmd->command_id % app->node_count == app->node_id) {
    if (app->node_id == 0)
    {
        // print_addr(&req->cliaddr);
        int n = sendto(app->paxos->sock, retval, content_length(req), 0,
                        (struct sockaddr *)&req->cliaddr,
                        sizeof(req->cliaddr));
        if (n < 0)
            perror("deliver: sendto error");
    }
}

void usage(char *prog) {
    printf("Usage: %s configuration-file learner_id number_of_learner [enable_leveldb]\n", prog);
}

static void
learner_thread_free(struct learner_thread* l, struct application_ctx* app)
{
    //bufferevent_free
    event_free(l->ev_perf);
    event_base_free(l->ctx->base);
    free_paxos_ctx(l->ctx);
    free_paxos_ctx(app->paxos);
    free(app->proxies);
    free_leveldb_context(app->leveldb);
    free(app);
    paxos_log_debug("Exit properly");
    free(l);
}


static void*
start_thread(void* v)
{
    int learner_id = *((int*)v);
    paxos_log_debug("Learner thread %d: starting....\n", learner_id);
    struct learner_thread* l = malloc (sizeof(struct learner_thread));

    struct application_ctx *app = malloc(sizeof (struct application_ctx));

    app->node_id = node_id;
    app->node_count = node_count;
    app->at_second = 0;
    app->message_per_second = 0;
    app->enable_leveldb = enable_leveldb;
    app->amount_of_write = amount_of_write;
    app->proxies = calloc(conf.proposer_count, sizeof(struct sockaddr_in));
    int i;
    for (i = 0; i < conf.proposer_count; i++) {
        ip_to_sockaddr( conf.proposer_address[i],
                    conf.proposer_port[i],
                    &app->proxies[i] );
    }
    //start learner thread
    l = make_learner(learner_id, &conf, deliver, app);
    learners[learner_id] = l;
    app->paxos = l->ctx;
    if (app->enable_leveldb)
        app->leveldb = common_levelb;
    
    l->ev_perf = event_new(l->ctx->base, -1, EV_TIMEOUT|EV_PERSIST, on_perf, app);
    struct timeval one_second = {1, 0};
    event_add(l->ev_perf, &one_second);

    event_base_priority_init(l->ctx->base, 4);
    event_priority_set(l->ev_perf, 0);
    
    //start paxos in learner (event_base_dispatch)
    start_paxos(app->paxos);
    //event_base_dispatch(l->ctx->base);
    learner_thread_free(l, app);
    pthread_exit(NULL);
}

static void 
start_learner(int * learner_id, pthread_t* t)
{
    int rc = 0;
    if ((rc = pthread_create(t, NULL, start_thread, learner_id)))
    {       
        fprintf(stderr, "error: pthread_create, rc: %d\n",rc);
    }
}

static void
finish_learner(struct learner_thread* l)
{
    event_base_loopbreak(l->ctx->base);
}

static void
sigint_handler(int sig)
{
  int i;
  printf("Caught signal %d\n", sig);
  for (i = 0; i <  NUM_OF_THREAD; i++) 
  {
    finish_learner(learners[i]);
  }
}

int main(int argc, char *argv[])
{

    pthread_t* t;
    int i, *ids;
    // Initialize mutex
    
    pthread_mutex_init(&execute_mutex, NULL);
    pthread_cond_init(&execute, NULL);
    if (argc < 4) 
    {
        usage(argv[0]);
        return 0;
    }

    file_config = argv[1];
    node_id = atoi (argv[2]);
    node_count = atoi(argv[3]);
    if (argc > 4)
    {
        enable_leveldb = atoi(argv[4]);
    }
    int percent_write = 5;
    if (argc > 5) 
    {
        percent_write = atoi(argv[5]);
    }
    if (percent_write == 0) {
        amount_of_write = 0;
    } else {
        /* Work for less than 50% of write */
        amount_of_write = 100 / percent_write;
    }
    
    srand(time(NULL));
    signal(SIGINT, sigint_handler);

    populate_configuration(file_config, &conf);
    dump_configuration(&conf);

    //start leveldb
    if (enable_leveldb)
        common_levelb = new_leveldb_context();

    t = malloc(NUM_OF_THREAD * sizeof(pthread_t));
    ids = malloc(NUM_OF_THREAD * sizeof(int));


    learners = malloc(NUM_OF_THREAD *sizeof(struct learner_thread*));
    for (i = 0; i < NUM_OF_THREAD; i++)
    {
        ids[i] = i;
        start_learner(&ids[i], &t[i]);
    }
   

    for (i = 0; i < NUM_OF_THREAD; i++)
    {
        pthread_join(t[i], NULL);
        paxos_log_debug("Learner thread %d finished!\n", ids[i]);
    }
      /* Clean up and exit */
    pthread_mutex_destroy(&execute_mutex);

    free(t);
    free(ids);
    free_configuration(&conf);
    free(learners);
    

    return 0; 
}
