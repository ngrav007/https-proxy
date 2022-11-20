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

typedef struct Client {
    Query *query;
    struct sockaddr_in addr;  // Client address
    struct timeval last_recv; // Time of last activity
    socklen_t addr_l;         // Length of client address
    size_t buffer_l;          // Length of buffer
    char *buffer;             // Buffer for outgoing messages
    int socket;               // Client socket
    bool isSlow;              // True if sends partial messages
} Client;

Client *Client_new();
Client *Client_create(int socket);
int Client_init(Client *client, int socket);
void Client_free(void *client);
void Client_print(void *client);
int Client_compare(void *client1, void *client2);
int Client_setSocket(Client *client, int socket);
int Client_setAddr(Client *client, struct sockaddr_in *addr);
int Client_setLoggedIn(Client *client, bool loggedIn);
int Client_getSocket(Client *client);
const char *Client_getId(Client *client);
bool Client_isLoggedIn(Client *client);
bool Client_isSlowMofo(Client *client);
int Client_timestamp(Client *client);

#endif /* _CLIENT_H_ */
