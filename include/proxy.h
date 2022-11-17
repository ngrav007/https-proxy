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
    List *client_list;
    struct sockaddr_in addr;
    struct hostent *client; // TODO - add to client
    struct hostent *server;
    struct timeval *timeout; // ? do we need timeouts
    /* new members for handling multiple clients */
    fd_set master_set;
    fd_set readfds;
    int fdmax;
    char *client_ip;
    char *server_ip;
    char *buffer;
    size_t buffer_l;
    size_t buffer_sz;
    int listen_fd;
    int client_fd;
    int server_fd;
    short port;
} Proxy;

int Proxy_run(short port, size_t cache_size);
int Proxy_init(Proxy *proxy, short port, size_t cache_size);
void Proxy_free(void *proxy);
void Proxy_print(Proxy *proxy);
ssize_t Proxy_recv(Proxy *proxy, int socket);
ssize_t Proxy_send(Proxy *proxy, int socket);
int Proxy_handle(Proxy *proxy);
int Proxy_accept(Proxy *proxy);
int Proxy_handleListener(Proxy *proxy);
int Proxy_handleClient(Proxy *proxy, Client *client);
int Proxy_handleTimeout(Proxy *proxy);
int Proxy_errorHandle(Proxy *proxy, int error_code);
void Proxy_close_socket(int socket, fd_set *master_set, List *client_list, Client *client);

ssize_t Proxy_fetch(Proxy *proxy, char *hostname, int port, char *request, int request_len);

#endif /* _PROXY_H_ */
