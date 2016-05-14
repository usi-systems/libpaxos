#ifndef NET_UTILS_H_
#define NET_UTILS_H_

#include <netinet/in.h>

struct address
{
    char* addr;
    int port;
};

struct sockaddr_in* address_to_sockaddr_in(struct address *a);
int create_server_socket(struct sockaddr_in *myaddr);

#endif