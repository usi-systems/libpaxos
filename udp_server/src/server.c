#include <stdio.h>
#include <stdlib.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "paxos.h"

#include "datastore.h"

struct rocksdb_params rocks;
struct rocksdb_configurations rocksdb_configurations;

struct server_context {
    struct event_base *base;
    int at_second;
    int message_per_second;
    struct sockaddr_in server_addr;
};

void handle_signal(evutil_socket_t fd, short what, void *arg)
{
    struct server_context *ctx = arg;
    event_base_loopbreak(ctx->base);
}

void on_perf(evutil_socket_t fd, short event, void *arg) {
    struct server_context *ctx = arg;
    printf("%4d %8d\n", ctx->at_second++, ctx->message_per_second);
    ctx->message_per_second = 0;
}

static void deliver(unsigned int worker_id, unsigned int  inst,
                     char *val,  size_t size, void *arg) {
    if (rocks.num_workers == 1) {
        worker_id = 0;
    }

    struct request *ap = (struct request *)val;
    // printf("worker %u inst %d, type: %d, key %u, value %u\n", worker_id, inst, ap->type, ap->key, ap->value);
    if (ap->type == WRITE_REQ) {
        uint32_t key_len = KEYLEN;   // rte_be_to_cpu_32(ap->key_len);
        uint32_t value_len = VALLEN; // rte_be_to_cpu_32(ap->value_len);
        // printf("Key %s, Value %s\n", ap->key, ap->value);
        // // Single PUT
        handle_put(rocks.worker[worker_id].db, rocks.writeoptions, (const char *)&ap->req.write.key,
                    key_len, (const char *)&ap->req.write.value, value_len);
        rocks.worker[worker_id].write_count++;

    } else if (ap->type == READ_REQ) {
        size_t len;
        uint32_t key_len = KEYLEN; // rte_be_to_cpu_32(ap->key_len);
        // printf("Key %s\n", ap->key);
        char *returned_value =
        handle_get(rocks.worker[worker_id].db, rocks.readoptions, (const char *)&ap->req.write.key, key_len, &len);
        if (returned_value != NULL) {
            // printf("Key %s: return value %s\n", ap->key, returned_value);
            memcpy((char *)&ap->req.write.value, returned_value, len);
            free(returned_value);
        }
        rocks.worker[worker_id].read_count++;
    }

    rocks.worker[worker_id].delivered_count++;

    if (rocksdb_configurations.enable_checkpoint) {
        char cp_path[FILENAME_LENGTH];
        snprintf(cp_path, FILENAME_LENGTH, "%s/checkpoints/%s-core-%u-inst-%u",
        rocks.worker[worker_id].db_path, rocks.hostname, worker_id, inst);
        handle_checkpoint(rocks.worker[worker_id].cp, cp_path);
    }
}


void send_trim_message(evutil_socket_t fd, uint32_t inst, struct server_context *ctx) {
    struct paxos_hdr trim;
    trim.msgtype = LEARNER_CHECKPOINT;
    trim.inst = inst;
    trim.rnd = 0;
    trim.vrnd = 0;
    trim.worker_id = 0;
    // print_paxos_hdr(&trim);
    serialize_paxos_hdr(&trim);
    int send_bytes = sendto(fd, &trim, sizeof(trim), 0, (struct sockaddr *)&ctx->server_addr,
                            sizeof(ctx->server_addr));
    if (send_bytes < 0) {
        perror("sendto");
        return;
    }
    // printf("Send Trim inst %u\n", inst);
}

void on_read(evutil_socket_t fd, short event, void *arg) {
    struct server_context *ctx = arg;
    struct sockaddr_in remote;
    socklen_t addrlen = sizeof(remote);
    // char buf[1024];
    // int recv_bytes = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*)&remote, &addrlen);
    struct paxos_hdr msg;
    int recv_bytes = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr*)&remote, &addrlen);

    if (recv_bytes < 0) {
        perror("recvfrom");
        return;
    }
    deserialize_paxos_hdr(&msg);
    ctx->message_per_second++;
    int send_bytes;
    deliver(msg.worker_id, msg.inst, (char*)&msg.value, sizeof(msg.value), &rocks);
    if (msg.inst % 16384 == 0) {
        send_trim_message(fd, msg.inst, ctx);
    }
    // print_paxos_hdr(&msg);
    serialize_paxos_hdr(&msg);
    send_bytes = sendto(fd, &msg, recv_bytes, 0, (struct sockaddr*)&remote, addrlen);
    if (send_bytes < 0) {
        perror("sendto");
        return;
    }
}

int create_server_socket(int port) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        perror("create socket server");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(0);
    addr.sin_port = htons(port);
    int res = bind(s, (struct sockaddr*)&addr, sizeof(addr));
    if (res < 0)
        perror("bind socket");

    return s;
}

void subcribe_to_multicast_group(char *group, int sockfd)
{
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sockfd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq))<0) {
        perror("IP_ADD_MEMBERSHIP");
        exit(EXIT_FAILURE);
    }
}


/******************************************************************
 * Checks if specified IP is multicast IP.
 *
 * Returns 0 if specified IP is not multicast IP, else non-zero.
 *
 * Parameters:
 * ip IP to check for multicast IP, stored in network byte-order.
 ******************************************************************/
int net_ip__is_multicast_ip(char *ip_address)
{
    in_addr_t ip_in_addr = inet_addr(ip_address);
    char *ip = (char *)&ip_in_addr;
    int i = ip[0] & 0xFF;

    if(i >=  224 && i <= 239){
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int ret = parse_rocksdb_configuration(argc, argv);
    if (ret < 0) {
      rocksdb_print_usage();
      return -1;
    }
    argc -= ret;
    argv += ret;

    struct server_context ctx;
    ctx.base = event_base_new();
    ctx.at_second = 0;
    ctx.message_per_second = 0;
    int sock = create_server_socket(9081);
    evutil_make_socket_nonblocking(sock);

    if (argc > 1) {
        if (net_ip__is_multicast_ip(argv[1])) {
            subcribe_to_multicast_group(argv[1], sock);
        }
    }
    struct hostent *server;
    server = gethostbyname(argv[2]);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    memset(&ctx.server_addr, 0, sizeof ctx.server_addr);
    ctx.server_addr.sin_family = AF_INET;
    memcpy((char *)&(ctx.server_addr.sin_addr.s_addr), (char *)server->h_addr, server->h_length);
    ctx.server_addr.sin_port = htons(9081);

    rocks.num_workers = 1;
    print_parameters();
    init_rocksdb(&rocks);

    struct event *ev_read, *ev_perf, *ev_signal;
    ev_read = event_new(ctx.base, sock, EV_READ|EV_PERSIST, on_read, &ctx);
    event_add(ev_read, NULL);

    ev_perf = event_new(ctx.base, -1, EV_TIMEOUT|EV_PERSIST, on_perf, &ctx);
    struct timeval one_second = {1, 0};
    event_add(ev_perf, &one_second);

    ev_signal = evsignal_new(ctx.base, SIGINT|SIGTERM, handle_signal, &ctx);
    evsignal_add(ev_signal, NULL);
    event_base_priority_init(ctx.base, 4);
    event_priority_set(ev_signal, 0);
    event_priority_set(ev_perf, 2);
    event_priority_set(ev_read, 3);
    event_base_dispatch(ctx.base);

    event_free(ev_read);
    event_free(ev_perf);
    event_free(ev_signal);
    event_base_free(ctx.base);

    return 0;
}
