#include <stdio.h>
#include <stdlib.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include "message.h"
#include <math.h>
#define BILLION 1000000000
#define ON 1
#define OFF 0

struct client_context {
    struct event_base *base;
    struct sockaddr_in server_addr;
    enum Operation op;
    uint16_t command_id;
    int sock;
};

static int at_second = 0;
static int num_messages = 0;
static int flag = 0;
static struct timespec time_sum = {0, 0};
static struct timespec last_time = {0,0};
static struct timeval tv = {0, 360000};
static int timeout_flag = 0;

int timespec_diff(struct timespec *result, struct timespec *end,struct timespec *start)
{
    if (end->tv_nsec < start->tv_nsec) {
        result->tv_nsec =  BILLION + end->tv_nsec - start->tv_nsec;
        result->tv_sec = end->tv_sec - start->tv_sec - 1;
    } else {
        result->tv_nsec = end->tv_nsec - start->tv_nsec;
        result->tv_sec = end->tv_sec - start->tv_sec;
    }
  /* Return 1 if result is negative. */
  return end->tv_sec < start->tv_sec;
}

struct timespec timespec_add(struct timespec * time1, struct timespec *time2)
{
    struct timespec sum;
    sum.tv_sec = time1->tv_sec + time2->tv_sec;
    sum.tv_nsec = time1->tv_nsec + time2->tv_nsec;
    if(sum.tv_nsec >= BILLION)
    {
        sum.tv_sec++;
        sum.tv_nsec = sum.tv_nsec - BILLION;
    }
    return sum;
}
double timespec_double(struct timespec time)
{
    return ((double) time.tv_sec + ((double)time.tv_nsec / BILLION));
}
void handle_signal(evutil_socket_t fd, short what, void *arg)
{
    printf("Caught signal\n");
    struct client_context *ctx = arg;
    event_base_loopbreak(ctx->base);
}

void send_to_addr(struct client_context *ctx, int timeout_flag) {
    socklen_t addr_size = sizeof (ctx->server_addr);
    struct command cmd;
    cmd.command_id = ctx->command_id++;
    cmd.op = ctx->op;
    if(timeout_flag == 1)
        cmd.ts = last_time;
    else if (timeout_flag == 0)
    {
        clock_gettime(CLOCK_REALTIME, &cmd.ts);
        last_time = cmd.ts;
        
    }
    memset(cmd.content, 'k', 15);
    cmd.content[15] = '\0';
    memset(cmd.content+16, 'v', 15);
    cmd.content[31] = '\0';
    int msg_size = sizeof (cmd);
    int n = sendto(ctx->sock , &cmd, msg_size, 0, (struct sockaddr *)&ctx->server_addr, addr_size);
    if (n < 0) {
        perror("sendto");
    }
    
}

void on_read(evutil_socket_t fd, short event, void *arg) {
    struct client_context *ctx = arg;

    if (event&EV_READ) {
        struct sockaddr_in remote;
        socklen_t addrlen = sizeof(remote);
        struct command response;

        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)) < 0)
        {
            printf ("Hurrar -recvfrom() timeout.\n");
            timeout_flag = 1;
        }
        else {
            int recvlen = recvfrom(fd, &response, sizeof(response), 0, (struct sockaddr*)&remote, &addrlen);
            if (recvlen < 0)
                perror ("recvfrom\n");
            struct timespec end;
            clock_gettime(CLOCK_REALTIME, &end);
            struct timespec result;
            if(flag == ON)
            {
                if (timespec_diff(&result, &end, &response.ts) < 1)
                    printf("%ld.%09ld\n", result.tv_sec, result.tv_nsec);

            }
            else if (flag == OFF)
            {
                if (timespec_diff(&result, &end, &response.ts) < 1)
                {
                    time_sum = timespec_add(&time_sum, &result);
                    num_messages++;
                }

            }
            timeout_flag = 0;

        }
        /*int recvlen = recvfrom(fd, &response, sizeof(response), 0, (struct sockaddr*)&remote, &addrlen);
        if (recvlen >= 0){

            struct timespec end;
            clock_gettime(CLOCK_REALTIME, &end);
            struct timespec result;
            if(flag == ON)
            {
                if (timespec_diff(&result, &end, &response.ts) < 1)
                    printf("%ld.%09ld\n", result.tv_sec, result.tv_nsec);

            }
            else if (flag == OFF)
            {
                if (timespec_diff(&result, &end, &response.ts) < 1)
                {
                    time_sum = timespec_add(&time_sum, &result);
                    num_messages++;
                }

            }
            timeout_flag = 0;
        }
        else {
            printf("recvlen: %d, errno: %d EAGAIN %d EWOULDBLOCK %d \n", recvlen, errno, EAGAIN, EWOULDBLOCK);
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                printf ("Hurrar -recvfrom() timeout.\n");
                timeout_flag = 1;
            }
        }   */
    }
    send_to_addr(ctx, timeout_flag);
}
void on_perf (evutil_socket_t fd, short event, void *arg) {
    if (flag == OFF)
    {
        double sum = 0.0, agv_latency = 0;
        sum = timespec_double(time_sum);
        if (num_messages == 0 || sum == 0)
             printf("%4d %6d 0\n", at_second++, num_messages);
        else
        {
            agv_latency = sum / num_messages;
            printf("%4d %6d %f\n", at_second++, num_messages, agv_latency);
            num_messages= 0;
            time_sum.tv_sec = 0;
            time_sum.tv_nsec = 0;

        }
         
    }
    
}
int new_dgram_socket() {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("New DGRAM socket");
        return -1;
    }
    return s;
}


int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Syntax: %s [hostname] [port] [GET/SET] [ON/OFF]\n"
               "Example: %s 192.168.1.110 6789 GET OFF\n",argv[0], argv[0]);
        return 1;
    }

    struct hostent *server;
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    struct client_context ctx;
    ctx.op = GET;
    flag = OFF;
    if (argc > 3) {
        if (strcmp(argv[3], "SET") == 0) {
            ctx.op = SET;
        }
        if (strcmp(argv[4], "ON") == 0){
            flag = ON;
        }
    }

    ctx.command_id = 0;
    memset(&ctx.server_addr, 0, sizeof ctx.server_addr);
    ctx.server_addr.sin_family = AF_INET;
    memcpy((char *)&(ctx.server_addr.sin_addr.s_addr), (char *)server->h_addr, server->h_length);
    ctx.server_addr.sin_port = htons(atoi(argv[2]));

    ctx.base = event_base_new();
    int sock = new_dgram_socket();
    evutil_make_socket_nonblocking(sock);
    ctx.sock = sock;
    struct event *ev_read, *ev_sigint, *ev_sigterm, *ev_latency;
    ev_read = event_new(ctx.base, sock, EV_READ|EV_PERSIST, on_read, &ctx);
    event_add(ev_read, NULL);

    ev_latency = event_new(ctx.base, -1, EV_TIMEOUT|EV_PERSIST, on_perf, &ctx);
    struct timeval one_second = {1, 0};
    event_add(ev_latency, &one_second);

    ev_sigint = evsignal_new(ctx.base, SIGINT, handle_signal, &ctx);
    evsignal_add(ev_sigint, NULL);

    ev_sigterm = evsignal_new(ctx.base, SIGTERM, handle_signal, &ctx);
    evsignal_add(ev_sigterm, NULL);

    send_to_addr(&ctx, timeout_flag);

    event_base_dispatch(ctx.base);

    event_free(ev_read);
    event_free(ev_sigint);
    event_free(ev_sigterm);
    event_base_free(ctx.base);

    return 0;
}
