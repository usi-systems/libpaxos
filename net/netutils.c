#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

int new_dgram_socket() {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("New DGRAM socket");
        return -1;
    }
    return s;
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

void ip_to_sockaddr(const char *host, int port, struct sockaddr_in *saddr) {
    saddr->sin_family = AF_INET;
    saddr->sin_addr.s_addr = inet_addr(host);
    saddr->sin_port = htons(port);
}