#ifndef _PROXY_H_
#define _PROXY_H_

#include "config.h"
#include "client.h"
#include "colors.h"

#include "http.h"
#include "list.h"
#include "utility.h"

#if RUN_CACHE == 1
#include "cache.h"
#endif

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

#if RUN_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
// #include <ssl.h>
// #include <err.h>
#endif 

typedef struct Proxy {
    #if RUN_CACHE 
        Cache *cache;
    #endif 

    List *client_list;

    #if RUN_SSL
        SSL_CTX *ctx;
    #endif 

    struct sockaddr_in addr;
    struct hostent *client;  // ? do we need this?
    struct hostent *server;  // ? do we need this?
    struct timeval *timeout; // ? do we need timeouts
    fd_set master_set;
    fd_set readfds;
    char *client_ip;
    char *server_ip;
    int fdmax;
    int listen_fd;
    int client_fd;
    int server_fd;
    short port;
} Proxy;

int Proxy_run(short port, size_t cache_size);
int Proxy_init(Proxy *proxy, short port, size_t cache_size);
void Proxy_free(void *proxy);
void Proxy_print(Proxy *proxy);

int Proxy_listen(Proxy *proxy);
int Proxy_accept(Proxy *proxy);
ssize_t Proxy_send(int socket, char *buffer, size_t buffer_l);
int Proxy_sendError(Client *client, int msg_code);
ssize_t Proxy_recv(Proxy *proxy, int socket);
ssize_t Proxy_fetch(Proxy *proxy, Query *request);
void Proxy_close(int socket, fd_set *master_set, List *client_list, Client *client);

int Proxy_handle(Proxy *proxy);
int Proxy_handleListener(Proxy *proxy);
int Proxy_handleClient(Proxy *proxy, Client *client);
int Proxy_handleQuery(Proxy *proxy, Query *query);
int Proxy_handleTimeout(Proxy *proxy);
int Proxy_handleEvent(Proxy *proxy, Client *client, int error_code);
int Proxy_handleConnect(int sender, int receiver);

#if RUN_SSL
    int Proxy_handleSSL(Proxy *proxy, Client *client);
    int Proxy_SSLconnect(Proxy *proxy, Query *query);
#endif 

#endif /* _PROXY_H_ */
