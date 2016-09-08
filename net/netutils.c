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

void
hexdump(char *buf, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        if (i % 16 == 0)
            printf("\n");
        printf("%02x ", (unsigned char)buf[i]);
    }
    printf("\n");
}
