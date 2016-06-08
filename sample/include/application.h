#ifndef APPLICATION_H_
#define APPLICATION_H_

 /* levelDB */
#include <leveldb/c.h>

#define MAX_KEY_SIZE 16
#define MAX_VALUE_SIZE 16
#define TMP_VALUE_SIZE 32

struct leveldb_request
{
    int op;
    size_t ksize;
    char key[MAX_KEY_SIZE];
    size_t vsize;
    char value[MAX_VALUE_SIZE];
};

struct echo_request
{
    char value[32];
};

enum application_t {
    SIMPLY_ECHO,
    LEVELDB
};

union request {
    struct echo_request echo;
    struct leveldb_request leveldb;
} request;


struct client_value
{
    int proxy_id;
    int request_id;
    int size;
    char content[TMP_VALUE_SIZE];
};

enum command_t {
    PUT = 1,
    GET,
    DELETE
};

enum boolean { false, true };


#endif