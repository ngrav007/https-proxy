#ifndef __CHATCONFIG_H__
#define __CHATCONFIG_H__

#define DEBUG 1

#define LISTEN_BACKLOG 5
#define TIMEOUT        60 // 60 seconds

#define BUFFER_SZ   512 // read buffer size
#define HEADER_SZ   50  // 50 bytes
#define MAX_DATA_SZ 400 // 400 bytes
#define MAX_ID_SZ   20  // 20 bytes
#define MAX_MSG_SZ  450

#define HELLO_MSG     1   // HELLO message
#define HELLO_ACK     2   // HELLO ACK message
#define LIST_REQUEST  3   // LIST REQUEST message
#define CLIENT_LIST   4   // CLIENT LIST message
#define CHAT_MSG      5   // CHAT message
#define EXIT_MSG      6   // EXIT message
#define ECLIENTEXISTS 7   // Error: Client already exists
#define ECANTDELIVER  8   // Error: Can't deliver message
#define HALT          666 // Halt message

#define SERVER_ID   "Server"
#define SERVER_ID_L sizeof(SERVER_ID) - 1

#define CLIENT_TIMEDOUT  6
#define CLIENT_CLOSED    0
#define ERROR_FAILURE    -1
#define ERROR_CLOSE      -2
#define INVALID_CLIENT   -3
#define INVALID_MESSAGE  -4
#define CLIENT_EXISTS    -5
#define CLIENT_NOT_FOUND -6
#define CANT_DELIVER     -7
#define PARTIAL_MESSAGE  -8
#define OVERFLOW_MESSAGE -9

#endif /* __CHATCONFIG_H__ */
