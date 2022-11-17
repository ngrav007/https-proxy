#ifndef __PROXYCONFIG_H__
#define __PROXYCONFIG_H__

#include <limits.h>

#define DEBUG 1

/* Proxy */
#define DEFAULT_MAX_AGE 3600
#define LISTEN_BACKLOG 5
#define TIMEOUT        60 // 60 seconds
#define HALT 666 // Halt message

/* Utility */
#define BUFFER_SZ 1024 // default buffer size

/* Cache */
#define CACHE_SZ 10

/* HTTP */
#define HTTP_PORT "80"
#define HTTP_PORT_L 2
#define HEADER_END "\r\n\r\n"
#define HEADER_END_L 4
#define CRLF  "\r\n"
#define CRLF_L 2
#define HTTP_GET "get"
#define HTTP_GET_L 3
#define HTTP_CONNECT "connect"
#define HTTP_CONNECT_L 7

/* HTTP Header Fields */
#define CONTENT_LEN "content-length:"
#define CONTENT_LEN_L 15
#define HOST "host:" /* "host:" appears in localhost:9010 in first line */
#define HOST_L 5
#define CACHECONTROL "cache-control:"
#define CACHECONTROL_L 14
#define MAXAGE "max-age="
#define MAXAGE_L 8

/* Indicators */
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

/* Limits */
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

#endif /* __PROXYCONFIG_H__ */
