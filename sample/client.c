/*
 * Copyright (c) 2013-2015, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <paxos.h>
#include <evpaxos.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <event2/event.h>
#include <netinet/tcp.h>

/* levelDB */
 #include <leveldb/c.h>

#define MAX_KEY_SIZE 16
#define MAX_VALUE_SIZE 16


struct client_value
{
  struct timeval t;
  int op;
  int client_id;
  size_t ksize;
  char key[MAX_KEY_SIZE];
  size_t vsize;
  char value[MAX_VALUE_SIZE];
};

struct stats
{
	long min_latency;
	long max_latency;
	long avg_latency;
	int delivered_count;
	size_t delivered_bytes;
};

struct client
{
	int id;
    int client_id;
    struct client_value v;
	int outstanding;
	char* send_buffer;
	struct stats stats;
	struct event_base* base;
	struct bufferevent* bev;
	struct event* stats_ev;
	struct timeval stats_interval;
	struct event* sig;
	struct evlearner* learner;
	FILE *fp;
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
};

enum command_t {
    PUT = 1,
    GET,
    DELETE
};

enum boolean { false, true };

void open_db(struct client *ctx, char* db_name) {
    char *err = NULL;
    ctx->options = leveldb_options_create();
    leveldb_options_set_create_if_missing(ctx->options, true);
    ctx->db = leveldb_open(ctx->options, db_name, &err);    
    if (err != NULL) {
        fprintf(stderr, "Open fail.\n");
        leveldb_free(err);
        exit (EXIT_FAILURE);
    }
}

void destroy_db(struct client *ctx, char* db_name) {
    char *err = NULL;
    leveldb_destroy_db(ctx->options, db_name, &err);    
    if (err != NULL) {
        fprintf(stderr, "Open fail.\n");
        leveldb_free(err);
        exit (EXIT_FAILURE);
    }
}

int add_entry(struct client *ctx, int sync, char *key, int ksize, char* val, int vsize) {
    char *err = NULL;
    leveldb_writeoptions_set_sync(ctx->woptions, sync);
    leveldb_put(ctx->db, ctx->woptions, key, ksize, val, vsize, &err);
    if (err != NULL) {
        fprintf(stderr, "Write fail.\n");
        leveldb_free(err); err = NULL;
        return(1);
    }
    return 0;
}

int get_value(struct client *ctx, char *key, size_t ksize, char** val, size_t* vsize) {
    char *err = NULL;
    *val = leveldb_get(ctx->db, ctx->roptions, key, ksize, vsize, &err);
    if (err != NULL) {
        fprintf(stderr, "Read fail.\n");
        leveldb_free(err); err = NULL;
        return(1);
    }
    return 0;
}

int delete_entry(struct client *ctx, char *key, int ksize) {
    char *err = NULL;
    leveldb_delete(ctx->db, ctx->woptions, key, ksize, &err);
    if (err != NULL) {
        fprintf(stderr, "Delete fail.\n");
        leveldb_free(err); err = NULL;
        return(1);
    }
    return 0;
}


static void
handle_sigint(int sig, short ev, void* arg)
{
	struct event_base* base = arg;
	printf("Caught signal %d\n", sig);
	event_base_loopexit(base, NULL);
}

static void
random_string(char *s, const int len)
{
	int i;
	static const char alphanum[] =
		"0123456789abcdefghijklmnopqrstuvwxyz";
	for (i = 0; i < len-1; ++i)
		s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
	s[len-1] = 0;
}

static void
client_submit_value(struct client* c)
{
	gettimeofday(&c->v.t, NULL);
	c->v.client_id = c->client_id;
    size_t size = sizeof(struct timeval) + sizeof(int) + 2*sizeof(size_t) + c->v.ksize + c->v.vsize;
    paxos_submit(c->bev, (char*)&c->v, size);
}

// Returns t2 - t1 in microseconds.
static long
timeval_diff(struct timeval* t1, struct timeval* t2)
{
	long us;
	us = (t2->tv_sec - t1->tv_sec) * 1e6;
	if (us < 0) return 0;
	us += (t2->tv_usec - t1->tv_usec);
	return us;
}

static void
update_stats(struct stats* stats, struct client_value* delivered, size_t size)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	long lat = timeval_diff(&delivered->t, &tv);
	printf("%ld\n", lat);
	stats->delivered_count++;
	stats->delivered_bytes += size;
	stats->avg_latency = stats->avg_latency + 
		((lat - stats->avg_latency) / stats->delivered_count);
	if (stats->min_latency == 0 || lat < stats->min_latency)
		stats->min_latency = lat;
	if (lat > stats->max_latency)
		stats->max_latency = lat;
}

static void
on_deliver(unsigned iid, char* value, size_t size, void* arg)
{
	struct client* c = arg;
	struct client_value* v = (struct client_value*)value;
	if (c->client_id != v->client_id) {
		return;
	}
	switch(v->op) {
		case PUT: {
			// printf("PUT: key %s: %zu, value %s: %zu\n", v->key, v->ksize, v->value, v->vsize);
			add_entry(c, false, v->key, v->ksize, v->value, v->vsize);
			break;
		}
		case GET: {
			char *value;
			size_t vsize = 0;
			get_value(c, v->key, v->ksize, &value, &vsize);
			// printf("GET: key %s: %zu, RETURN: %s %zu\n", v->key, v->ksize, value, vsize);
			if (value)
				free(value);
			break;
		}
	}
	update_stats(&c->stats, v, size);
	client_submit_value(c);
}

static void
on_stats(evutil_socket_t fd, short event, void *arg)
{
	struct client* c = arg;
	double mbps = (double)(c->stats.delivered_bytes * 8) / (1024*1024);
    fprintf(c->fp, "%d value/sec, %.2f Mbps, latency min %ld us max %ld us avg %ld us\n",
		c->stats.delivered_count, mbps, c->stats.min_latency,
		c->stats.max_latency, c->stats.avg_latency);
	memset(&c->stats, 0, sizeof(struct stats));
	event_add(c->stats_ev, &c->stats_interval);
}

static void
on_connect(struct bufferevent* bev, short events, void* arg)
{
	int i;
	struct client* c = arg;
	if (events & BEV_EVENT_CONNECTED) {
		printf("Connected to proposer\n");
		for (i = 0; i < c->outstanding; ++i)
			client_submit_value(c);
	} else {
		printf("%s\n", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
	}
}

static struct bufferevent* 
connect_to_proposer(struct client* c, const char* config, int proposer_id)
{
	struct bufferevent* bev;
	struct evpaxos_config* conf = evpaxos_config_read(config);
	if (conf == NULL) {
		printf("Failed to read config file %s\n", config);
		return NULL;
	}
	struct sockaddr_in addr = evpaxos_proposer_address(conf, proposer_id);
	bev = bufferevent_socket_new(c->base, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, NULL, NULL, on_connect, c);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
	bufferevent_socket_connect(bev, (struct sockaddr*)&addr, sizeof(addr));
	int flag = 1;
	setsockopt(bufferevent_getfd(bev), IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
	return bev;
}

static struct client*
make_client(const char* config, int proposer_id, int outstanding, int client_id, int op, char *key, char *value)
{
	struct client* c;
	c = malloc(sizeof(struct client));
	c->base = event_base_new();
	
	memset(&c->stats, 0, sizeof(struct stats));
	c->bev = connect_to_proposer(c, config, proposer_id);
	if (c->bev == NULL)
		exit(1);
	
	c->outstanding = outstanding;
	c->client_id = client_id;
	c->send_buffer = malloc(sizeof(struct client_value));
	c->stats_interval = (struct timeval){1, 0};
	c->stats_ev = evtimer_new(c->base, on_stats, c);
	event_add(c->stats_ev, &c->stats_interval);
	
	paxos_config.learner_catch_up = 0;
	c->learner = evlearner_init(config, on_deliver, c, c->base);
	
	c->sig = evsignal_new(c->base, SIGINT, handle_sigint, c->base);
	evsignal_add(c->sig, NULL);
	
	gettimeofday(&c->v.t, NULL);

	char fname[128];
	snprintf(fname, 128, "client%d-osd%d.txt", client_id, outstanding);
	c->fp = fopen(fname, "w+");
	/* craft levelDB request */
	c->v.op = op;
	if (key != NULL) {
		size_t ksize = strlen(key) + 1;
		c->v.ksize = ksize;
		memcpy(c->v.key, key, ksize);
	} else {
		c->v.ksize = MAX_KEY_SIZE;
		random_string(c->v.key, c->v.ksize);
	}
	if (value != NULL) {
		size_t vsize = strlen(value) + 1;
		c->v.vsize = vsize;
		memcpy(c->v.value, value, vsize);
	} else {
		c->v.vsize = MAX_VALUE_SIZE;
		random_string(c->v.value, c->v.vsize);
	}
	c->v.client_id = client_id;
	fprintf(c->fp, "key: %s\nvalue: %s\n", c->v.key, c->v.value);
    c->woptions = leveldb_writeoptions_create();
    c->roptions = leveldb_readoptions_create();
	return c;
}

static void
client_free(struct client* c)
{
	free(c->send_buffer);
	bufferevent_free(c->bev);
	event_free(c->stats_ev);
	event_free(c->sig);
	event_base_free(c->base);
	if (c->learner)
		evlearner_free(c->learner);
	fclose(c->fp);
	leveldb_close(c->db);
    leveldb_writeoptions_destroy(c->woptions);
    leveldb_readoptions_destroy(c->roptions);
    leveldb_options_destroy(c->options);
	free(c);
}

static void
start_client(const char* config, int proposer_id, int outstanding, int  client_id, int op, char *key, char *value)
{
	struct client* client;
	client = make_client(config, proposer_id, outstanding, client_id, op, key, value);
	signal(SIGPIPE, SIG_IGN);
    open_db(client, "/tmp/test");
	event_base_dispatch(client->base);
	client_free(client);
}

static void
usage(const char* name)
{
	printf("Usage: %s [path/to/paxos.conf] [-h] [-o] [-v] [-p]\n", name);
	printf("  %-30s%s\n", "-h, --help", "Output this message and exit");
	printf("  %-30s%s\n", "-o, --outstanding #", "Number of outstanding client values");
	// printf("  %-30s%s\n", "-v, --value-size #", "Size of client value (in bytes)");
	printf("  %-30s%s\n", "-p, --proposer-id #", "d of the proposer to connect to");
	exit(1);
}

int
main(int argc, char const *argv[])
{
	int i = 1;
	int proposer_id = 0;
	int outstanding = 1;
	int op = GET;
	int client_id = 0;
	char *key = NULL;
	char *value = NULL;
	if (argc < 2) {
		usage(argv[0]);
		return 0;
    }

	while (i != argc) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			usage(argv[0]);
		else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--outstanding") == 0)
			outstanding = atoi(argv[++i]);
		else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--proposer-id") == 0)
			proposer_id = atoi(argv[++i]);
		else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--client-id") == 0)
			client_id = atoi(argv[++i]);
		else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--set-op") == 0)
			op = atoi(argv[++i]);
		else if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--set-key") == 0)
			key = strdup(argv[++i]);
		else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--set-value") == 0)
			value = strdup(argv[++i]);
		i++;
	}
	
	srand(time(NULL));
	start_client(argv[1], proposer_id, outstanding, client_id, op, key, value);
	if (key)
		free(key);
	if (value)
		free(value);
	return 0;
}
