#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "learner.h"

#define NUM_ACCEPTORS 3
#define INSTANCE_ID 0
#define MAX_INSTANCE 10E6

int force_quit;

void *produce_accepted(void *arg);
void *consume_accepted(void *arg);
void *print_stat(void *arg);

struct producer_parm {
    int producer_id;
    struct learner* learner;
    int delivered_total;
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
    force_quit = 0;

    struct timespec start;
    struct timespec end;


    clock_gettime(CLOCK_REALTIME, &start);
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
    clock_gettime(CLOCK_REALTIME, &end);
    struct timespec result;
    timespec_diff(&result, &end, &start);
    float timelap = (float)(NS_PER_S*result.tv_sec + result.tv_nsec);
    for (i = 0; i < NUM_THREADS; i++) {
      float speed = (float)parms[i].delivered_total / timelap * NS_PER_S;
      printf("Thread %d: Throughput: %3.f / %3.f = %.3f\n", i, (float)parms[i].delivered_total, timelap, speed);
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
    int local_iid = parm->producer_id;

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
    do
    {
        for (i = 0; i < NUM_ACCEPTORS; i++) {
            msg.aid = i;
            // printf("Thread %d submit instance %d acceptor %d.\n", parm->producer_id, msg.iid, msg.aid);
            learner_receive_accepted(parm->learner, &msg);
        }
        local_iid += 1;
        msg.iid = local_iid;

        if(learner_deliver_next(parm->learner, &out))
            parm->delivered_total++;
        // printf("Thread %d: current delivered instance %d\n", parm->producer_id, out.iid);
    }
    while (out.iid < MAX_INSTANCE - 1 && !force_quit);
    /* Stop all threads */
    force_quit = 1;
    return NULL;
}
