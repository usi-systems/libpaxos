#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <leveldb/c.h>
#include <pthread.h>
#include <time.h>

struct leveldb_ctx {
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
};
struct leveldb_ctx* new_leveldb_context();
void open_db(struct leveldb_ctx *ctx, char* db_name);
void destroy_db(struct leveldb_ctx *ctx, char* db_name);
int add_entry(struct leveldb_ctx *ctx, char *key, int ksize,
                char* val, int vsize);
int get_value(struct leveldb_ctx *ctx, char *key, size_t ksize, char** val,
                size_t* vsize);
int delete_entry(struct leveldb_ctx *ctx, char *key, int ksize);
void free_leveldb_context(struct leveldb_ctx *ctx);
void *consumer_task(void *arg);

#define TEST_DB_BASE "/tmp/test_leveldb"
#define NUM_KEYS 10000000
enum boolean {
    false,
    true
};

struct leveldb_ctx* new_leveldb_context() {
    struct leveldb_ctx *ctx = malloc(sizeof (struct leveldb_ctx));
    ctx->options = leveldb_options_create();
    ctx->woptions = leveldb_writeoptions_create();
    leveldb_writeoptions_set_sync(ctx->woptions, false);
    ctx->roptions = leveldb_readoptions_create();
    return ctx;
}


void open_db(struct leveldb_ctx *ctx, char* db_name) {
    char *err = NULL;
    leveldb_options_set_create_if_missing(ctx->options, true);
    ctx->db = leveldb_open(ctx->options, db_name, &err);
    if (err != NULL) {
        fprintf(stderr, "Open failed: %s\n", err);
        exit (EXIT_FAILURE);
    }
    leveldb_free(err); err = NULL;
}

void destroy_db(struct leveldb_ctx *ctx, char* db_name) {
    char *err = NULL;
    leveldb_destroy_db(ctx->options, db_name, &err);
    if (err != NULL) {
        fprintf(stderr, "Destroy failed: %s\n", err);
        exit (EXIT_FAILURE);
    }
    leveldb_free(err); err = NULL;
}

int add_entry(struct leveldb_ctx *ctx, char *key, int ksize, char* val, int vsize) {
    char *err = NULL;
    leveldb_put(ctx->db, ctx->woptions, key, ksize, val, vsize, &err);
    if (err != NULL) {
        fprintf(stderr, "Write fail.\n");
        return(1);
    }
    leveldb_free(err); err = NULL;
    return 0;
}

int get_value(struct leveldb_ctx *ctx, char *key, size_t ksize, char** val, size_t* vsize) {
    char *err = NULL;
    *val = leveldb_get(ctx->db, ctx->roptions, key, ksize, vsize, &err);
    if (err != NULL) {
        fprintf(stderr, "Read fail.\n");
        return(1);
    }
    leveldb_free(err); err = NULL;
    return 0;
}

int delete_entry(struct leveldb_ctx *ctx, char *key, int ksize) {
    char *err = NULL;
    leveldb_delete(ctx->db, ctx->woptions, key, ksize, &err);
    if (err != NULL) {
        fprintf(stderr, "Delete fail.\n");
        return(1);
    }
    leveldb_free(err); err = NULL;
    return 0;
}

void free_leveldb_context(struct leveldb_ctx *ctx) {
    leveldb_writeoptions_destroy(ctx->woptions);
    leveldb_readoptions_destroy(ctx->roptions);
    leveldb_options_destroy(ctx->options);
    free(ctx);
}

int main(int argc, char* argv[])
{
  if (argc < 2) {
    printf("Usage: %s number-of-thread\n", argv[0]);
    exit(0);
  }
  int NUM_THREADS = atoi(argv[1]);
  struct leveldb_ctx* ctxs[NUM_THREADS];

  size_t db_name_length = strlen(TEST_DB_BASE) + 3;
  char db_names[NUM_THREADS][db_name_length];
  int i, ret;
  for (i = 0; i < NUM_THREADS; i++)
  {
    sprintf(db_names[i], "%s%02d", TEST_DB_BASE, i);
  }

  pthread_t consumers[NUM_THREADS];

  for (i = 0; i < NUM_THREADS; i++)
  {
      struct leveldb_ctx *ctx = new_leveldb_context();
      ctxs[i] = ctx;
      open_db(ctx, db_names[i]);
      ret = pthread_create(&consumers[i], NULL, consumer_task, (void*)ctx);
      if (ret) {
          fprintf(stderr, "Error - pthread_create() return code %d\n", ret);
      }
  }


  for (i = 0; i < NUM_THREADS; i++)
  {
      pthread_join(consumers[i], NULL);
      char *stat = leveldb_property_value(ctxs[i]->db, "leveldb.stats");
      printf("%s\n", stat);
      free(stat);
      // char *sstable = leveldb_property_value(ctxs[i]->db, "leveldb.sstables");
      // printf("%s\n", sstable);
      // free(sstable);
      leveldb_close(ctxs[i]->db);
      destroy_db(ctxs[i], db_names[i]);
      free_leveldb_context(ctxs[i]);
  }
  return 0;
}

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

void *consumer_task(void *arg)
{
  struct leveldb_ctx *ctx = (struct leveldb_ctx *)arg;
  pthread_t tid = pthread_self();

  const char *value = "value";
  char key[64] = {0};
  struct timespec start;
  struct timespec end;
  clock_gettime(CLOCK_REALTIME, &start);
  char *err = NULL;

  for (int i=0; i < NUM_KEYS; i++)
  {
    snprintf(key, 64, "thread%03ld-key%018d", tid, i+1);
    leveldb_put(ctx->db, ctx->woptions, key, strlen(key), value, strlen(value) + 1, &err);
    if (err != NULL) {
        fprintf(stderr, "Write fail.\n");
        break;
    }
    leveldb_free(err); err = NULL;
  }
  clock_gettime(CLOCK_REALTIME, &end);
  struct timespec result;
  timespec_diff(&result, &end, &start);
  float timelap = (float)(NS_PER_S*result.tv_sec + result.tv_nsec);
  float speed = (float)NUM_KEYS / timelap * NS_PER_S;
  printf("Thread %ld: Throughput: %d / %3.f = %.3f\n", tid, NUM_KEYS, timelap, speed);
  return NULL;
}
