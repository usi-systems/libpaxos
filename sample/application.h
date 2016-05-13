#ifndef APPLICATION_H_
#define APPLICATION_H_

#define MAX_KEY_SIZE 16
#define MAX_VALUE_SIZE 16

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
    int client_id;
    struct timeval depart_ts;
    enum application_t application_type;
    int size;
    union request content;
};

enum command_t {
    PUT = 1,
    GET,
    DELETE
};

enum boolean { false, true };

#endif