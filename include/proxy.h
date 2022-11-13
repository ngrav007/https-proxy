#ifndef _PROXY_H_
#define _PROXY_H_

#include "cache.h"
#include "client.h"
#include "colors.h"
#include "config.h"
#include "http.h"
#include "list.h"
#include "utility.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

typedef struct Connection {
    struct HTTP_Request *request;
    struct HTTP_Response *response;
    bool is_from_cache;
} Connection;

typedef struct Proxy {
    Cache *cache;
    struct sockaddr_in addr;
    // struct sockaddr_in client_addr;
    struct sockaddr_in server_addr;
    struct hostent *client; // TODO - add to client
    struct hostent *server;
    char *client_ip;
    char *server_ip;
    char *buffer;
    size_t buffer_l;
    size_t buffer_sz;
    int listen_fd; // mastersocket in a multiclient proxy
    int client_fd;
    int server_fd;
    short port;

    /* new members for handling multiple clients */
    fd_set master_set;
    fd_set readfds;
    int fdmax;
    struct timeval *timeout; // ?? do we need timeouts

    /* persisting client list */
    List *client_list;
} Proxy;

int Proxy_run(short port, size_t cache_size);
int Proxy_init(Proxy *proxy, short port, size_t cache_size);
void Proxy_free(void *proxy);
void Proxy_print(Proxy *proxy);
ssize_t Proxy_read(Proxy *proxy, int socket);
ssize_t Proxy_write(Proxy *proxy, int socket);
int Proxy_handle(Proxy *proxy);

Connection *Connection_new(struct HTTP_Request *request,
                           struct HTTP_Response *response);
void Connection_init(Connection *conn, struct HTTP_Request *request,
                     struct HTTP_Response *response);
void Connection_free(void *conn);
void Connection_print(void *conn);

int Proxy_accept_new_client(Proxy *proxy);
int Proxy_handleListener(Proxy *proxy);
int Proxy_handleClient(Proxy *proxy, Client *client);
int Proxy_handleTimeout(Proxy *proxy);
int Proxy_errorHandle(Proxy *proxy, int error_code);

#endif /* _PROXY_H_ */
