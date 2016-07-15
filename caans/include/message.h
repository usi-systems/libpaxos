#ifndef _MESSAGE_H_
#define _MESSAGE_H_
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> 

struct command {
    struct timespec ts;
    int command_id;
    char content[16];
};


struct __attribute__((__packed__)) client_request {
    unsigned length;
    struct sockaddr_in cliaddr;
    char content[1];
};

unsigned content_length(struct client_request *request);
unsigned message_length(struct client_request *request);
struct client_request* create_client_request(const char *data, unsigned data_size);
void print_message(struct client_request *request);
void hexdump_message(struct client_request *request);
#endif