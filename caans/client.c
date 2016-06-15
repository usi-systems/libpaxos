#include <event2/dns.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/event.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#include "message.h"

#define BILLION 1000000000
#define DATA_SIZE 16

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

int
timespec_diff(struct timespec *result, struct timespec *end,struct timespec *start)
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

void send_request(struct bufferevent *bev) {
    char data_to_send[DATA_SIZE];
    random_string(data_to_send, DATA_SIZE);
    struct client_request *request = create_client_request(data_to_send, DATA_SIZE);
    clock_gettime(CLOCK_REALTIME, &request->ts);
    bufferevent_write(bev, request, request->length);
    // printf("%-10s: content: %s -- size: %d\n", "Send", request->content,
        // message_length(request));
    // hexdump_message(request);
    free(request);
}

void readcb(struct bufferevent *bev, void *ptr)
{
   struct evbuffer *input = bufferevent_get_input(bev);
    char buffer[128];
    int n;
    n = evbuffer_remove(input, buffer, sizeof(buffer));
    if (n < 0)
        return;
    struct client_request *response = (struct client_request *)buffer;
    // printf("%-10s: content: %s -- size: %d\n", "Receive", response->content,
        // message_length(response));
    // hexdump_message(response);

    struct timespec end, result;
    clock_gettime(CLOCK_REALTIME, &end);
    timespec_diff(&result, &end, &response->ts);
    printf("%ld.%.9ld\n", result.tv_sec, result.tv_nsec);

    send_request(bev);
}

void eventcb(struct bufferevent *bev, short events, void *ptr)
{
    if (events & BEV_EVENT_CONNECTED) {
        return;
    } else if (events & (BEV_EVENT_ERROR|BEV_EVENT_EOF)) {
        struct event_base *base = ptr;
        if (events & BEV_EVENT_ERROR) {
            int err = bufferevent_socket_get_dns_error(bev);
            if (err)
                printf("DNS error: %s\n", evutil_gai_strerror(err));
        }
        bufferevent_free(bev);
        event_base_loopexit(base, NULL);
    }
    if (events & BEV_EVENT_TIMEOUT) {
        bufferevent_enable(bev, EV_READ|EV_WRITE);
        send_request(bev);
    }
}

void handle_signal(evutil_socket_t fd, short what, void *arg) {
    struct event_base *base = arg;
    event_base_loopbreak(base);
}

int main(int argc, char* argv[])
{
    struct event_base *base;
    struct evdns_base *dns_base;
    struct bufferevent *bev;

    if (argc != 3) {
        printf("Syntax: %s [hostname] [port]\n"
               "Example: %s 192.168.1.110 6789\n",argv[0], argv[0]);
        return 1;
    }

    base = event_base_new();
    dns_base = evdns_base_new(base, 1);

    struct event *ev_signal = evsignal_new(base, SIGINT|SIGTERM, handle_signal, base);
    evsignal_add(ev_signal, NULL);

    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, readcb, NULL, eventcb, base);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
    struct timeval one_second = {1, 0};
    bufferevent_set_timeouts(bev, &one_second, NULL);
    send_request(bev);

    bufferevent_socket_connect_hostname(
        bev, dns_base, AF_INET, argv[1], atoi(argv[2]));
    event_base_dispatch(base);

    event_free(ev_signal);
    evdns_base_free(dns_base, 1);
    event_base_free(base);

    return 0;
}