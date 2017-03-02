#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "message.h"
#include <time.h>
uint16_t content_length(struct client_request *request)
{
    return request->length - (sizeof(struct client_request) - 1);
}

uint16_t message_length(struct client_request *request)
{
    return request->length;
}

void get_threadid (uint16_t *out, char *p, int offset)
{
    uint16_t *raw_bytes = (uint16_t *)(p + offset);
    *out = *raw_bytes;
}

struct client_request* create_client_request(char *data, uint16_t data_size, uint16_t *tid)
{
    uint16_t message_size = sizeof(struct client_request) + data_size - 1;
    struct client_request *request = (struct client_request*)malloc(message_size);
    request->length = message_size;

    int offset = sizeof(struct timespec) + sizeof(uint16_t);
    get_threadid (tid, data, offset);

    memcpy(request->content, data, data_size);

    return request;
}

void print_message(struct client_request *request)
{
    printf("Length %d\n", request->length);
    // printf("%ld.%.9ld\n", request->ts.tv_sec, request->ts.tv_nsec);
    printf("Content %s\n", request->content);
}

void hexdump_message(struct client_request *request)
{
    int i;
    char *data = (char *)request;
    for (i = 0; i < request->length; i++) {
        if (i % 16 ==0)
            printf("\n");
        printf("%02x ", (unsigned char)data[i]);
    }
    printf("\n");
}

unsigned long
hash(const char *s)
{
    unsigned long h;
    unsigned const char *us;

    /* cast s to unsigned const char * */
    /* this ensures that elements of s will be treated as having values >= 0 */
    us = (unsigned const char *) s;

    h = 0;
    while(*us != '\0') {
        h = h * MULTIPLIER + *us;
        us++;
    } 

    return h;
}
