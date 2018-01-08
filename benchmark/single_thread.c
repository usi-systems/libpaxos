#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "learner.h"

#define NUM_ACCEPTORS 3
#define INSTANCE_ID 0
#define NUM_THREADS NUM_ACCEPTORS
#define MAX_INSTANCE 10E7

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

int deliver_per_second;
int force_quit;

void *print_stat(void *arg);

int main(int argc, char* argv[])
{
    struct learner* learner = learner_new(NUM_ACCEPTORS);
    learner_set_instance_id(learner, INSTANCE_ID);
    pthread_t stat_thread;
    int i, ret;
    deliver_per_second = 0;
    force_quit = 0;

    ret = pthread_create(&stat_thread, NULL, print_stat, NULL);
    if (ret)
        fprintf(stderr, "pthread_create error. Return code %d\n", ret);

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
        .aid = 0,
        .value = v
    };


    paxos_accepted out;

    do {
        for (i = 0; i < NUM_ACCEPTORS; i++) {
            msg.aid = i;
            learner_receive_accepted(learner, &msg);
        }


        if (learner_deliver_next(learner, &out))
            deliver_per_second++;

        msg.iid = ++local_iid;
    } while (out.iid < MAX_INSTANCE && out.iid != MAX_INSTANCE - 1);

    /* Stop all threads */
    force_quit = 1;

    learner_free(learner);

    exit(EXIT_SUCCESS);
}


void *print_stat(void *arg)
{
    while (!force_quit)
    {
        sleep(1);
        pthread_mutex_lock(&mutex1);

        float tp = (float) deliver_per_second / 10E3;
        printf("Throughput: %f Kps\n", tp);
        deliver_per_second = 0;

        pthread_mutex_unlock(&mutex1);
    }
    return NULL;
}
