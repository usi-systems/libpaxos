#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "learner.h"

#define NUM_ACCEPTORS 3
#define INSTANCE_ID 1
#define MAX_INSTANCE 10000000

void *produce_accepted(void *arg);
void *consume_accepted(void *arg);
void *print_stat(void *arg);

struct producer_parm {
    int producer_id;
    struct learner* learner;
    int delivered_total;
    char log_to_file;
};

#define NS_PER_S 10E9

int timespec_diff(struct timespec *result, struct timespec *end, struct timespec *start)
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

int main(int argc, char* argv[])
{
    // struct learner* learner = learner_new(NUM_ACCEPTORS);
    if (argc < 2) {
      printf("Usage: %s number-of-thread\n", argv[0]);
      exit(0);
    }
    int NUM_THREADS = atoi(argv[1]);
    struct producer_parm parms[NUM_THREADS];
    pthread_t consumers[NUM_THREADS];
    int i, ret;

    for (i = 0; i < NUM_THREADS; i++)
    {
        parms[i].learner = learner_new(NUM_ACCEPTORS);
        learner_set_instance_id(parms[i].learner, INSTANCE_ID);
        parms[i].producer_id = i;
        parms[i].delivered_total = 0;
        ret = pthread_create(&consumers[i], NULL, consume_accepted, (void*)&parms[i]);
        if (ret)
        {
            fprintf(stderr, "Error - pthread_create() return code %d\n", ret);
        }
        /*
        ret = pthread_create(&stat_thread, NULL, print_stat, (void*)&parms[i]);
        if (ret)
        {
          fprintf(stderr, "Error - pthread_create() return code %d\n", ret);
        }
        */
    }


    for (i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(consumers[i], NULL);
        learner_free(parms[i].learner);
    }
    exit(EXIT_SUCCESS);
}

/*
 * This thread function creates accepted messages simulating acceptor's job.
 */
void *consume_accepted(void *arg)
{
    struct producer_parm* parm = arg;
    char hello_word[] = "Hello";
    int local_iid = 0;

    struct paxos_value v = {
        .paxos_value_len = 6,
        .paxos_value_val = hello_word
    };

    struct paxos_accepted msg = {
        .iid = local_iid,
        .ballot = 0,
        .value_ballot = 0,
        .aid = parm->producer_id,
        .value = v
    };

    int i;

    paxos_accepted out;
    FILE *fp;
    if (parm->log_to_file) {
      char log_fn[10];
      sprintf(log_fn, "thr%d.txt", parm->producer_id);
      fp = fopen(log_fn, "w+");
    }
    struct timespec start;
    struct timespec end;
    clock_gettime(CLOCK_REALTIME, &start);
    do
    {
        for (i = INSTANCE_ID; i < NUM_ACCEPTORS; i++) {
            msg.aid = i;
            if (parm->log_to_file) {
              fprintf(fp, "Thread %d submit instance %d acceptor %d.\n", parm->producer_id, msg.iid, msg.aid);
            }
            learner_receive_accepted(parm->learner, &msg);
        }
        if(learner_deliver_next(parm->learner, &out)) {
          parm->delivered_total++;
          if (parm->log_to_file) {
            fprintf(fp, "Thread %d: current delivered instance %d\n", parm->producer_id, out.iid);
          }
        }
        local_iid += 1;
        msg.iid = local_iid;
    }
    while (out.iid < MAX_INSTANCE && msg.iid < 3*MAX_INSTANCE);

    clock_gettime(CLOCK_REALTIME, &end);
    struct timespec result;
    timespec_diff(&result, &end, &start);
    float timelap = (float)(NS_PER_S*result.tv_sec + result.tv_nsec);
    float speed = parm->delivered_total / timelap * NS_PER_S;
    printf("Thread %d: Throughput: %3.f / %3.f = %.3f\n", parm->producer_id, (float)parm->delivered_total, timelap, speed);
    if (parm->log_to_file) {
      fclose(fp);
    }
    return NULL;
}
