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

typedef struct Query {
    Request *req;
    Response *res;
    struct sockaddr_in server_addr;
    struct hostent *host_info;
    struct timeval timestamp;
    socklen_t server_addr_l;
    int socket;

    char *buffer;
    size_t buffer_l;
    size_t buffer_sz;
} Query;

Query *Query_new(char *buffer, size_t buffer_l);
Query *Query_create(Request *req, Response *res,
                    struct sockaddr_in *server_addr, socklen_t server_addr_l,
                    int server_fd);
void Query_free(Query *query);
void Query_print(Query *query);
int Query_compare(Query *query1, Query *query2);

#endif /* _Query_H_ */
