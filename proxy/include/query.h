#ifndef _Query_H_
#define _Query_H_

#include "config.h"
#include "http.h"
#include "utility.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>

#if RUN_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif 

#define QUERY_HTTPS 1
#define QUERY_HTTP  0

typedef struct Query {
    Request *req;
    Response *res;

    #if RUN_SSL
        SSL *ssl;
        SSL_CTX *ctx;
    #endif 

    struct sockaddr_in server_addr;
    struct hostent *host_info;
    struct timeval timestamp;
    socklen_t server_addr_l;
    int socket;
    char *buffer;
    size_t buffer_l;
    size_t buffer_sz;
    int gotHeader;
    int bytes_left;
    int state;
} Query;

int Query_new(Query **q, char *buffer, size_t buffer_l);
void Query_free(Query *query);
void Query_print(Query *query);
int Query_compare(Query *query1, Query *query2);

#if RUN_SSL
void Query_clearSSLCtx(Query *query);
void Query_clearSSL(Query *query);
#endif 


#endif /* _Query_H_ */
