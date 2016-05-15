#ifndef NETUTILS_H_
#define NETUTILS_H_

#include <netinet/in.h>

int new_dgram_socket();
int create_server_socket(int port);
void ip_to_sockaddr(const char *host, int port, struct sockaddr_in *saddr);

#endif