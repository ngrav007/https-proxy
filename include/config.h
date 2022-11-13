#ifndef __PROXYCONFIG_H__
#define __PROXYCONFIG_H__

#include <limits.h>

#define DEBUG 1

/* Utility */
#define BUFFER_SZ 1024 // default buffer size

/* Cache Configuration */
#define CACHE_SZ 10

/* HTTP */
#define CRLF_L 2

#define LISTEN_BACKLOG 5
#define TIMEOUT        60 // 60 seconds

#define HEADER_SZ   50  // 50 bytes
#define MAX_DATA_SZ 400 // 400 bytes
#define MAX_ID_SZ   20  // 20 bytes
#define MAX_MSG_SZ  450

#define HALT 666 // Halt message

#define SERVER_ID   "Server"
#define SERVER_ID_L sizeof(SERVER_ID) - 1

/* indicators */
#define RECVD_BODY       2
#define RECVD_HEAD       1
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

#endif /* __PROXYCONFIG_H__ */
