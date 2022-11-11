#ifndef _CHATCLIENT_H_
#define _CHATCLIENT_H_

#include "ChatConfig.h"
#include "ChatHeader.h"
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

typedef struct ChatClient {
    ChatHeader header;        // header for most recent message
    struct sockaddr_in addr;  // Client address
    struct timeval last_recv; // Time of last activity
    socklen_t addr_l;         // Length of client address
    unsigned buffer_l;        // Length of buffer
    int socket;               // Client socket
    bool slowMofo;            // True if sends partial messages
    bool loggedIn;            // True if logged in
    char id[MAX_ID_SZ];       // Client ID
    char buffer[MAX_MSG_SZ];  // Buffer for outgoing messages
} ChatClient;

ChatClient *ChatClient_new();
ChatClient *ChatClient_create(int socket, char *id);
int ChatClient_init(ChatClient *client, char *id, int socket);
void ChatClient_free(void *client);
void ChatClient_print(void *client);
int ChatClient_compare(void *client1, void *client2);
int ChatClient_setHeader(ChatClient *client, char *buffer, int length);
int ChatClient_setSocket(ChatClient *client, int socket);
int ChatClient_setAddr(ChatClient *client, struct sockaddr_in *addr);
int ChatClient_setId(ChatClient *client, char *id);
int ChatClient_setLoggedIn(ChatClient *client, bool loggedIn);
int ChatClient_getSocket(ChatClient *client);
const char *ChatClient_getId(ChatClient *client);
bool ChatClient_isLoggedIn(ChatClient *client);
bool ChatClient_isSlowMofo(ChatClient *client);
int ChatClient_timestamp(ChatClient *client);

#endif /* _CHATCLIENT_H_ */
