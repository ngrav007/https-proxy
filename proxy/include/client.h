#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "config.h"
#include "http.h"
#include "list.h"
#include "query.h"
#include "utility.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#if RUN_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif 

typedef struct Client {
    Query *query;

    #if RUN_SSL
    SSL *ssl;
    bool isSSL;              // True if client is using SSL
    #endif 

    struct sockaddr_in addr;  // Client address
    struct timeval last_active; // Time of last activity
    socklen_t addr_l;         // Length of client address
    size_t buffer_l;          // Length of buffer
    size_t buffer_sz;         // Size of buffer
    char *buffer;             // Buffer for outgoing messages
    int socket;               // Client socket
    int state;                // State of client
    bool hasRequest;          // True if client has a request
} Client;

Client *Client_new();
Client *Client_create(int socket);
int Client_init(Client *client, int socket);
void Client_free(void *client);
void Client_print(void *client);
int Client_compare(void *client1, void *client2);
int Client_setSocket(Client *client, int socket);
int Client_setAddr(Client *client, struct sockaddr_in *addr);
int Client_getSocket(Client *client);
int Client_timestamp(Client *client);
void Client_clearQuery(Client *client);

#if RUN_SSL
bool Client_isSSL(Client *client);
void Client_clearSSL(Client *client);
#endif 

#endif /* _CLIENT_H_ */
