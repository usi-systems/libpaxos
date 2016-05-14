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

#include "application.h"
#include "application_config.h"

struct client
{
	int id;
    int client_id;
    struct client_value v;
	int outstanding;
    int run_leveldb;
    int vsize;
    char* send_buffer;
	char* read_buffer;
	struct event_base* base;
    struct bufferevent* bev;
	struct bufferevent* *learner_bevs;
	struct event* sig;
	FILE *fp;
};


static void
handle_sigint(int sig, short ev, void* arg) {
	struct event_base* base = arg;
	printf("Caught signal %d\n", sig);
	event_base_loopexit(base, NULL);
}


static void
random_string(char *s, const int len) {
	int i;
	static const char alphanum[] =
		"0123456789abcdefghijklmnopqrstuvwxyz";
	for (i = 0; i < len-1; ++i)
		s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
	s[len-1] = 0;
}


static void
client_submit_value(struct client* c) {
	gettimeofday(&c->v.depart_ts, NULL);
	c->v.client_id = c->client_id;
    paxos_submit(c->bev, (char*)&c->v, c->v.size);
}


void timeval_diff(struct timeval* res, struct timeval* start, struct timeval* end) {
    res->tv_sec = end->tv_sec - start->tv_sec;
    res->tv_usec = end->tv_usec - start->tv_usec;
}


void on_read(struct bufferevent *bev, void *ctx) {
    struct client* c = ctx;
    bufferevent_read(bev, c->read_buffer, sizeof( struct client_value));
    struct client_value* v = (struct client_value*)c->read_buffer;
    if (c->client_id != v->client_id) {
        return;
    }
    struct timeval tv, res;
    gettimeofday(&tv, NULL);
    timeval_diff(&res, &v->depart_ts, &tv);
    printf("%lu.%.6lu\n", res.tv_sec, res.tv_usec);
    client_submit_value(c);
}


static void
on_connect(struct bufferevent* bev, short events, void* arg) {
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
connect_to_proposer(struct client* c, const char* config, int proposer_id) {
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


void
connect_to_learner(struct client* c, const char* config_file) {
    application_config *config = parse_configuration(config_file);
    c->learner_bevs = calloc(config->number_of_learners, sizeof(struct bufferevent*));

    if (config == NULL) {
        printf("Failed to connect to learners\n");
        return;
    }
    int i;
    for (i = 0; i < config->number_of_learners; i++) {
        struct sockaddr_in *learner_sockaddr = address_to_sockaddr_in(&config->learners[i]);
        c->learner_bevs[i] = bufferevent_socket_new(c->base, -1, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(c->learner_bevs[i], on_read, NULL, NULL, c);
        bufferevent_enable(c->learner_bevs[i], EV_READ);
        bufferevent_socket_connect(c->learner_bevs[i],
            (struct sockaddr*)learner_sockaddr, sizeof(*learner_sockaddr));
        int flag = 1;
        setsockopt(bufferevent_getfd(c->learner_bevs[i]), IPPROTO_TCP,
            TCP_NODELAY, &flag, sizeof(int));
    }
}


static struct client*
make_client(const char* config, int proposer_id, int outstanding, int client_id,
        int op, char *key, char *value, int run_leveldb, const char *config_file)
{
	struct client* c;
	c = malloc(sizeof(struct client));
	c->base = event_base_new();
	
    c->bev = connect_to_proposer(c, config, proposer_id);
    if (c->bev == NULL)
        exit(1);

    connect_to_learner(c, config_file);

    c->outstanding = outstanding;
    c->client_id = client_id;
    c->run_leveldb = run_leveldb;
    c->vsize = 32;
    c->send_buffer = malloc(sizeof(struct client_value));
    c->read_buffer = malloc(sizeof(struct client_value));
	
	paxos_config.learner_catch_up = 0;
	
	c->sig = evsignal_new(c->base, SIGINT, handle_sigint, c->base);
	evsignal_add(c->sig, NULL);
	
	gettimeofday(&c->v.depart_ts, NULL);

	char fname[128];
	snprintf(fname, 128, "client%d-osd%d.txt", client_id, outstanding);
	c->fp = fopen(fname, "w+");
    if (c->run_leveldb) {
        /* craft levelDB request */
        c->v.application_type = LEVELDB;
        c->v.client_id = client_id;
        struct leveldb_request req;
        req.op = op;
        if (key != NULL) {
            size_t ksize = strlen(key) + 1;
            req.ksize = ksize;
            memcpy(req.key, key, ksize);
        } else {
            req.ksize = MAX_KEY_SIZE;
            random_string(req.key, req.ksize);
        }
        if (value != NULL) {
            size_t vsize = strlen(value) + 1;
            req.vsize = vsize;
            memcpy(req.value, value, vsize);
    } else {
        req.vsize = MAX_VALUE_SIZE;
        random_string(req.value, req.vsize);
    }
    c->v.content = (union request) req;
    c->v.size = sizeof(c->v);
    fprintf(c->fp, "key: %s\nvalue: %s\n", req.key, req.value);
    } else {
        c->v.application_type = SIMPLY_ECHO;
        c->v.size = sizeof(struct echo_request);
        struct echo_request req;
        random_string(req.value, c->vsize);
        printf("%s\n", req.value);
        c->v.content = (union request) req;
        c->v.size = sizeof(c->v);
    }
	return c;
}


static void
client_free(struct client* c)
{
    free(c->send_buffer);
	free(c->read_buffer);
	bufferevent_free(c->bev);
	event_free(c->sig);
	event_base_free(c->base);
	fclose(c->fp);
	free(c);
}


static void start_client(const char* config, int proposer_id, int outstanding,
    int client_id, int op, char *key, char *value, int run_leveldb,
    const char *config_file)
{
	struct client* client;
	client = make_client(config, proposer_id, outstanding, client_id, op, key,
        value, run_leveldb, config_file);
	signal(SIGPIPE, SIG_IGN);
	event_base_dispatch(client->base);
	client_free(client);
}


static void usage(const char* name) {
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
    int run_leveldb = 0;
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
		else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--run-leveldb") == 0)
			run_leveldb = 1;
		i++;
	}

	srand(time(NULL));
	start_client(argv[1], proposer_id, outstanding, client_id, op, key, value, run_leveldb, argv[2]);
	if (key)
		free(key);
	if (value)
		free(value);
	return 0;
}
