#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "net_utils.h"

struct sockaddr_in* address_to_sockaddr_in(struct address *a) {
    struct sockaddr_in *addr = malloc( sizeof (struct sockaddr_in));
    memset (addr, 0, sizeof (struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = htonl(0);
    addr->sin_port = htons(a->port);
    return addr;
}

int create_server_socket(struct sockaddr_in *myaddr) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    bind(s, (struct sockaddr*)myaddr, sizeof(*myaddr));
    return s;
}