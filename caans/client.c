#include <event2/dns.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/event.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#define BILLION 1000000000


struct client_request {
    struct timespec ts;
};

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
    struct client_request request;
    clock_gettime(CLOCK_REALTIME, &request.ts);
    bufferevent_write(bev, &request, sizeof(request));
}

void readcb(struct bufferevent *bev, void *ptr)
{
    struct client_request response;
    int n;
    struct evbuffer *input = bufferevent_get_input(bev);
    n = evbuffer_remove(input, &response, sizeof(response));
    if (n < 0)
        return;

    struct timespec end, result;
    clock_gettime(CLOCK_REALTIME, &end);
    timespec_diff(&result, &end, &response.ts);
    printf("%ld.%.9ld\n", result.tv_sec, result.tv_nsec);

    send_request(bev);
}

void eventcb(struct bufferevent *bev, short events, void *ptr)
{
    if (events & BEV_EVENT_CONNECTED) {
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

    send_request(bev);

    bufferevent_socket_connect_hostname(
        bev, dns_base, AF_INET, argv[1], atoi(argv[2]));
    event_base_dispatch(base);

    event_free(ev_signal);
    evdns_base_free(dns_base, 1);
    event_base_free(base);

    return 0;
}