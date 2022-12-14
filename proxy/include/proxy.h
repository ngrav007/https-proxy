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
#endif 

typedef struct Proxy {
    #if RUN_CACHE 
        Cache *cache;
    #endif 
    #if RUN_SSL
        SSL_CTX *ctx;
    #endif 

    List *client_list;

    struct sockaddr_in addr;
    struct timeval *timeout;
    fd_set master_set;
    fd_set readfds;
    int fdmax;
    int listen_fd;
    short port;
} Proxy;

int Proxy_run(short port);
int Proxy_init(Proxy *proxy, short port);
void Proxy_free(void *proxy);
void Proxy_print(Proxy *proxy);

int Proxy_listen(Proxy *proxy);
int Proxy_accept(Proxy *proxy);
ssize_t Proxy_send(int socket, char *buffer, size_t buffer_l);
ssize_t Proxy_recv(void *sender, int sender_type);
int Proxy_sendError(Client *client, int msg_code);
ssize_t Proxy_fetch(Proxy *proxy, Query *request);
void Proxy_close(int socket, fd_set *master_set, List *client_list, Client *client);

int Proxy_handle(Proxy *proxy);
int Proxy_handleListener(Proxy *proxy);
int Proxy_handleClient(Proxy *proxy, Client *client);
int Proxy_handleQuery(Proxy *proxy, Query *query, int isSSL);
int Proxy_handleTimeout(Proxy *proxy);
int Proxy_handleEvent(Proxy *proxy, Client *client, int error_code);
int Proxy_handleGET(Proxy *proxy, Client *client);
int Proxy_handleCONNECT(Proxy *proxy, Client *client);
int Proxy_serveFromCache(Proxy *proxy, Client *client, long age, char *key);
int Proxy_handleTunnel(int sender, int receiver);
int Proxy_sendServerResp(Proxy *proxy, Client *client);
#if RUN_SSL
    int ProxySSL_connect(Proxy *proxy, Query *query);
    int ProxySSL_handshake(Proxy *proxy, Client * client);
    int ProxySSL_write(Proxy *proxy, Client *client, char *buf, int len);
    int ProxySSL_shutdown(Proxy *proxy, Client *client);
    int ProxySSL_read(void *sender, int sender_type);
#endif 
int Proxy_updateExtFile(Proxy *proxy, char *hostname);
int Proxy_updateContext(Proxy *proxy);

#endif /* _PROXY_H_ */
