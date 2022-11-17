#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "config.h"
#include "http.h"
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
    // ChatHeader header;        // header for most recent message // TODO
    // remove
    char *key;
    struct sockaddr_in addr;  // Client address
    struct timeval last_recv; // Time of last activity
    socklen_t addr_l;         // Length of client address
    size_t buffer_l;          // Length of buffer
    int socket;               // Client socket
    bool slowMofo;            // True if sends partial messages
    // bool loggedIn;            // True if logged in // TODO remove
    // char id[MAX_ID_SZ];       // Client ID // TODO remove
    char *buffer; // Buffer for outgoing messages
} Client;

Client *Client_new();
Client *Client_create(int socket);
int Client_init(Client *client, int socket);
void Client_free(void *client);
void Client_print(void *client);
int Client_compare(void *client1, void *client2);
int Client_setHeader(Client *client, char *buffer, int length);
int Client_setSocket(Client *client, int socket);
int Client_setAddr(Client *client, struct sockaddr_in *addr);
// int Client_setId(Client *client, char *id);
int Client_setLoggedIn(Client *client, bool loggedIn);
int Client_getSocket(Client *client);
const char *Client_getId(Client *client);
bool Client_isLoggedIn(Client *client);
bool Client_isSlowMofo(Client *client);
int Client_timestamp(Client *client);
int Client_setKey(Client *client, HTTP_Header *header);

#endif /* _CLIENT_H_ */
