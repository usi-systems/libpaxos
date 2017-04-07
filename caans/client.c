#include <stdio.h>
#include <stdlib.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "message.h"
#include "netpaxos.h"
#define NS_PER_S 1000000000
//#define AGGREGATE
#define NUM_OF_THREAD 2
#define ALL 65

int flag;
unsigned char key_array [] = {'x', 'y'};
unsigned char value_array[] = {'1', '2'};
time_t rawtime;
struct tm *info;
char buffer[1024];

struct client_context {
    struct event_base *base;
    struct sockaddr_in server_addr;
    enum Operation op;
    uint32_t command_id;
    uint32_t current_iid;
    uint16_t client_id;
    int sock;
    double latency;
    int nb_messages;
    unsigned char key[1];
    unsigned char value[1];
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
    if (cmd.op == SET)
    {
        unsigned char *key, *value;
        
        key = malloc(sizeof(unsigned char));
        value = malloc(sizeof(unsigned char));
        
        random_string(key);
        //*key = 'z';
        memset(cmd.content, *key, 15);
        
        //memset(cmd.content, key_array[flag], 15);
        cmd.content[15] = '\0';
        
        random_string(value);
        //*value = '3';
        memset(cmd.content+16, *value, 15);
        //memset(cmd.content+16, value_array[flag], 15);
        cmd.content[31] = '\0';
        cmd.thread_id = command_to_thread(key);
        //cmd.thread_id = command_to_thread(&key_array[flag]);
        //cmd.thread_id = flag;
       
        //ctx->thread_id = cmd.thread_id;
        cmd.client_id = ctx->client_id;
        clock_gettime(CLOCK_REALTIME, &cmd.ts);

        paxos_log_debug("SET key %c value %c thread_id_%d command_id %u current_iid %u\n",
                     *key, *value, cmd.thread_id, cmd.command_id, ctx->current_iid);

        /*paxos_log_debug("%sSET key %c value %c thread_id_%d command_id %u current_iid %u\n",
                    buffer, key_array[flag],value_array[flag], cmd.thread_id, cmd.command_id, ctx->current_iid);*/
        // flag = 1 - flag;
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
        //ctx->thread_id = cmd.thread_id;
        cmd.client_id = ctx->client_id;
        clock_gettime(CLOCK_REALTIME, &cmd.ts);
        paxos_log_debug ("GET key %c thread_id_%d\n", *key, cmd.thread_id);
    }
    else if (cmd.op == INC)
    {
        unsigned char *key1, *key2;
        key1 = malloc(sizeof(unsigned char));
        key2 = malloc(sizeof(unsigned char));
        random_string(key1);
        //*key1 = 'x';
        memset(cmd.content, *key1, 15);
        cmd.content[15] = '\0';
        random_string(key2);
        //*key2 = 'y';
        memset(cmd.content+16, *key2, 15);
        cmd.content[31] = '\0';
        cmd.thread_id = ALL;
        //ctx->thread_id = cmd.thread_id;
        cmd.client_id = ctx->client_id;
        clock_gettime(CLOCK_REALTIME, &cmd.ts);
        paxos_log_debug ("INC key %c %c thread_id_%d\n", *key1, *key2, cmd.thread_id);
    }
    

    int msg_size = sizeof(cmd);   
    /*printf("msg_size %d timespec %lu enum %lu msgz %ld\n", 
        msg_size, sizeof(cmd.ts), sizeof(cmd.op), sizeof (struct command));
    printf("cmd content %lu thread_id %lu command_id %lu client_id %lu\n",
     sizeof(cmd.content), sizeof(cmd.thread_id), sizeof(cmd.command_id), sizeof(cmd.client_id));*/
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

void on_read(evutil_socket_t fd, short event, void *arg)
{
    struct client_context *ctx = arg;
    if (event&EV_READ)
    {
        struct sockaddr_in remote;
        socklen_t addrlen = sizeof(remote);
        struct command response;
        int n = recvfrom(fd, &response, sizeof(response), 0, (struct sockaddr*)&remote, &addrlen);
        if (n < 0)
            perror("recvfrom");
        struct timespec end;
        clock_gettime(CLOCK_REALTIME, &end);
        struct timespec result;
        if ( timespec_diff(&result, &end, &response.ts) < 1)
        {
#ifdef AGGREGATE
            ctx->latency += result.tv_sec*NS_PER_S + result.tv_nsec;
            ctx->nb_messages++;
#else
            //printf("t_%d %ld.%09ld\n", response.thread_id, result.tv_sec, result.tv_nsec);s
            printf("%ld.%09ld\n", result.tv_sec, result.tv_nsec);
            //paxos_log_debug("received from leaner command_id %u thread_id_%u\n", response.command_id, response.thread_id);
#endif
        }
        if(response.command_id < ctx->current_iid)
        {
            paxos_log_debug("respone iid %u current_iid %u "
                "response thread_id_%u "
                "Do not send the next message\n",
                response.command_id, ctx->current_iid,
                response.thread_id);
            return;
        }
    }
    ctx->current_iid++;
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
  printf("  %-30s%s\n", "-i, --identifier", "SET/GET/INC");
  
  exit(1);
}

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL);
    int i = 1, port;
    char *identifier = NULL;
    int c_id;

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
        else if (strcmp(argv[i], "-id") == 0 || strcmp(argv[i], "--client-id") == 0)
            c_id = atoi(argv[++i]);
        i++;
    }
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (strcmp(identifier, "SET") == 0)
        ctx.op = SET;
    else if (strcmp(identifier, "GET") == 0)
        ctx.op = GET;
    else if (strcmp(identifier, "INC") == 0)
        ctx.op = INC;
    srand(time(NULL));
    flag = 0;
    ctx.command_id = 1;
    ctx.client_id = c_id;
    memset(&ctx.server_addr, 0, sizeof ctx.server_addr);
    ctx.server_addr.sin_family = AF_INET;
    memcpy((char *)&(ctx.server_addr.sin_addr.s_addr), (char *)server->h_addr, server->h_length);
    ctx.server_addr.sin_port = htons(port);
    
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
    struct timeval hundred_ms = {0, 250}; //250 microsecond
    struct event *ev_perf = event_new(ctx.base, -1, EV_TIMEOUT|EV_PERSIST, on_perf, &ctx);
    event_add(ev_perf, &hundred_ms);
#endif
    ctx.current_iid = 1;
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
