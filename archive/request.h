#ifndef _REQUEST_H_
#define _REQUEST_H_

#include "http.h"
#include "utility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

typedef struct Request {
    HTTP_Header header;
    Response response;
    struct sockaddr_in server_addr;
    socklen_t server_addr_l;
    int server_fd;
    struct timeval *timeout;
} Request;

Request *Request_new(char *method, char *path, char *host, char *body);
void Request_free(void *request);
void Request_print(void *request);
int Request_init(Request *request, char *method, char *path, char *host, char *body);
int Request_compare(void *request1, void *request2);

#endif
