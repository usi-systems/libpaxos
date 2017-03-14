#include <stdio.h>
#include <stdlib.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include "message.h"

#define NS_PER_S 1000000000
#define AGGREGATE
#define NUM_OF_THREAD 2
#define ALL 65

struct client_context {
    struct event_base *base;
    struct sockaddr_in server_addr;
    enum Operation op;
    uint16_t command_id;
    int sock;
    double latency;
    int nb_messages;
};

int
timespec_diff(struct timespec *result, struct timespec *end,struct timespec *start)
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


void handle_signal(evutil_socket_t fd, short what, void *arg)
{
    printf("Caught signal\n");
    struct client_context *ctx = arg;
    event_base_loopbreak(ctx->base);
}

void random_string(unsigned char *s)
{
    int n = rand() % ('z' - 'a' + 1);
    *s = n + 'a';
}

uint16_t command_to_thread(unsigned char *s)
{
    unsigned long r = hash(s);
    return ((r % NUM_OF_THREAD));
}

void send_to_addr(struct client_context *ctx) {
    socklen_t addr_size = sizeof (ctx->server_addr);
    struct command cmd;
    cmd.command_id = ctx->command_id++;
    cmd.op = ctx->op;
    if (cmd.op == SET){
        clock_gettime(CLOCK_REALTIME, &cmd.ts);

        unsigned char *key, *value;
        key = malloc(sizeof(unsigned char));
        value = malloc(sizeof(unsigned char));
        random_string(key);
        memset(cmd.content, *key, 15);
        cmd.content[15] = '\0';
        random_string(value);
        memset(cmd.content+16, *value, 15);
        cmd.content[31] = '\0';
        cmd.thread_id = command_to_thread(key);

        printf ("SET key %c value %c thread_id %d\n", *key, *value, cmd.thread_id);
        
    }
    else if (cmd.op == GET)
    {
        unsigned char *key;
        key = malloc(sizeof(unsigned char));
        random_string(key);
        memset(cmd.content, *key, 15);
        cmd.content[15] = '\0';
        memset(cmd.content+16, '\0', 15);
        cmd.content[31] = '\0';
        cmd.thread_id = command_to_thread(key);
        clock_gettime(CLOCK_REALTIME, &cmd.ts);
        printf ("GET key %c thread_id %d\n", *key, cmd.thread_id);
    }
    else if (cmd.op == INC)
    {
        clock_gettime(CLOCK_REALTIME, &cmd.ts);
        unsigned char *key1, *key2;
        key1 = malloc(sizeof(unsigned char));
        key2 = malloc(sizeof(unsigned char));
        random_string(key1);
        memset(cmd.content, *key1, 15);
        cmd.content[15] = '\0';
        random_string(key2);
        memset(cmd.content+16, *key2, 15);
        cmd.content[31] = '\0';
        cmd.thread_id = ALL;

        printf ("INC key %c %c thread_id %d\n", *key1, *key2, cmd.thread_id);
    }
    

    int msg_size = sizeof cmd;   
    int n = sendto(ctx->sock , &cmd, msg_size, 0, (struct sockaddr *)&ctx->server_addr, addr_size); //server_addr: dest addr (proxy addr)
    if (n < 0) {
        perror("sendto");
    }
}


void on_perf(evutil_socket_t fd, short event, void *arg) {
    struct client_context *ctx = arg;
    if (ctx->nb_messages > 0) {
        double average_latency = ctx->latency / ctx->nb_messages / NS_PER_S;
        printf("%.09f\n", average_latency);
    }
    ctx->latency = 0.0;
    ctx->nb_messages = 0;
}

void on_read(evutil_socket_t fd, short event, void *arg) {
    struct client_context *ctx = arg;
    if (event&EV_READ) {
        struct sockaddr_in remote;
        socklen_t addrlen = sizeof(remote);
        struct command response;
        int n = recvfrom(fd, &response, sizeof(response), 0, (struct sockaddr*)&remote, &addrlen);
        if (n < 0)
            perror("recvfrom");
        struct timespec end;
        clock_gettime(CLOCK_REALTIME, &end);
        struct timespec result;
        if ( timespec_diff(&result, &end, &response.ts) < 1) {
#ifdef AGGREGATE
            ctx->latency += result.tv_sec*NS_PER_S + result.tv_nsec;
            ctx->nb_messages++;
#else
            printf("%ld.%09ld\n", result.tv_sec, result.tv_nsec);
#endif
        }
    }
    send_to_addr(ctx);
    
}

int new_dgram_socket() {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("New DGRAM socket");
        return -1;
    }
    return s;
}
void
usage(const char* name)
{
  printf("Usage: %s [ip address of proxy] [-h] [-r] [-p]\n", name);
  printf("  %-30s%s\n", "-h, --help", "Output this message and exit");
  printf("  %-30s%s\n", "-r, --remote-ip-address", "the address of proxy");
  printf("  %-30s%s\n", "-p, --port", "port");
  /*printf("  %-30s%s\n", "-i, --identifier", "Operation SET/GET/INC");
  printf("  %-30s%s\n", "-o, --object", "Object X,Y,Z");
  printf("  %-30s%s\n", "-v, --object-value", "the value of object SET");*/
  
  exit(1);
}

int main(int argc, char *argv[])
{
    /*if (argc < 3) {
        printf("Syntax: %s [hostname] [port] [GET/SET/INC] X \n"
               "Example: %s 192.168.1.110 6789 GET X\n",argv[0], argv[0]);
        return 1;
    }*/
    int i = 1, port;
    //char *object = NULL, *object_value = NULL, 
    char *identifier = NULL;
    //char *name[] = {"GET", "SET", "INC"};

    struct hostent *server;
    struct client_context ctx;
    ctx.latency = 0.0;
    ctx.nb_messages = 0;
    ctx.op = SET;
    while (i != argc){
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            usage(argv[0]);
        else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--remote-ip-address") == 0)
            server = gethostbyname(argv[++i]);
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0)
            port = atoi(argv[++i]);
        else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--identifier") == 0)
            identifier = argv[++i];
        /*else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--object") == 0)
            object = argv[++i];
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--object-value") == 0)
            object_value = argv[++i];*/
        i++;
    }
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    /*if (strcmp(identifier, "SET") == 0)
        ctx.op = SET;
    else if (strcmp(identifier, "GET") == 0)
        ctx.op = GET;
    else if (strcmp(identifier, "INC") == 0)
        ctx.op = INC;*/
    srand(time(NULL));

    //identifier = name[rand() % (sizeof(name) /  sizeof (name[0]))];
    if (strcmp(identifier, "SET") == 0)
        ctx.op = SET;
    else if (strcmp(identifier, "GET") == 0)
        ctx.op = GET;
    else if (strcmp(identifier, "INC") == 0)
        ctx.op = INC;
    //ctx.op = INC;

    ctx.command_id = 0;
    memset(&ctx.server_addr, 0, sizeof ctx.server_addr);
    ctx.server_addr.sin_family = AF_INET;
    memcpy((char *)&(ctx.server_addr.sin_addr.s_addr), (char *)server->h_addr, server->h_length);
    ctx.server_addr.sin_port = htons(port);

    /*if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    struct client_context ctx;
    ctx.latency = 0.0;
    ctx.nb_messages = 0;
    srand(time(NULL));
    ctx.op = GET;
    if (argc > 3) {
        if (strcmp(argv[3], "SET") == 0) {
            ctx.op = SET;
        }
    }
    ctx.command_id = 0;
    memset(&ctx.server_addr, 0, sizeof ctx.server_addr);
    ctx.server_addr.sin_family = AF_INET;
    memcpy((char *)&(ctx.server_addr.sin_addr.s_addr), (char *)server->h_addr, server->h_length);
    ctx.server_addr.sin_port = htons(atoi(argv[2]));
    */
    ctx.base = event_base_new();
    int sock = new_dgram_socket();
    evutil_make_socket_nonblocking(sock);
    ctx.sock = sock;
    struct event *ev_read, *ev_sigint, *ev_sigterm;

    struct timeval one_second = {1, 0};
    ev_read = event_new(ctx.base, sock, EV_READ|EV_PERSIST|EV_TIMEOUT, on_read, &ctx);
    event_add(ev_read, &one_second);

    ev_sigint = evsignal_new(ctx.base, SIGINT, handle_signal, &ctx);
    evsignal_add(ev_sigint, NULL);

    ev_sigterm = evsignal_new(ctx.base, SIGTERM, handle_signal, &ctx);
    evsignal_add(ev_sigterm, NULL);
#ifdef AGGREGATE
    struct timeval hundred_ms = {0, 10000};
    struct event *ev_perf = event_new(ctx.base, -1, EV_TIMEOUT|EV_PERSIST, on_perf, &ctx);
    event_add(ev_perf, &hundred_ms);
#endif
    send_to_addr(&ctx);

    event_base_dispatch(ctx.base);

    event_free(ev_read);
    event_free(ev_sigint);
    event_free(ev_sigterm);
#ifdef AGGREGATE
    event_free(ev_perf);
#endif
    event_base_free(ctx.base);

    return 0;
}