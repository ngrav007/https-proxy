#ifndef _PROXY_H_
#define _PROXY_H_

#include "cache.h"
#include "colors.h"
#include "http.h"
#include "utility.h"

#include <arpa/inet.h>
#include <error.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#define BUFSIZE 1024
#define MEGABYTE 4194304    // 4 MB (average size of a webpage)
#define CACHE_SZ 10

struct Connection {
    struct HTTP_Request *request;
    struct HTTP_Response *response;
    bool is_from_cache;
};

struct Proxy {
    Cache cache;
    struct sockaddr_in addr;
    struct sockaddr_in client_addr;
    struct sockaddr_in server_addr;
    struct hostent *client;
    struct hostent *server;
    char *client_ip;
    char *server_ip;
    char *buffer;
    size_t buffer_l;
    size_t buffer_sz;
    int listen_fd;
    int client_fd;
    int server_fd;
    short port;
};

int Proxy_run(short port, size_t cache_size);
int Proxy_init(struct Proxy *proxy, short port, size_t cache_size);
void Proxy_free(void *proxy);
void Proxy_print(struct Proxy *proxy);
ssize_t Proxy_read(struct Proxy *proxy, int socket);
ssize_t Proxy_write(struct Proxy *proxy, int socket);
int Proxy_handle(struct Proxy *proxy);

struct Connection *Connection_new(struct HTTP_Request *request,
                                  struct HTTP_Response *response);
void Connection_init(struct Connection *conn, struct HTTP_Request *request,
                     struct HTTP_Response *response);
void Connection_free(void *conn);
void Connection_print(void *conn);

#endif /* _PROXY_H_ */
