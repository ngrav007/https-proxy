#ifndef __PROXYCONFIG_H__
#define __PROXYCONFIG_H__

#include <limits.h>

#define DEBUG 1
#define TRUE  1
#define FALSE 0

/* Proxy */
#define DEFAULT_MAX_AGE 3600
#define LISTEN_BACKLOG  5
#define TIMEOUT         60  // 60 seconds
#define HALT            666 // Halt message
#define PROXY_HALT      "__halt__"
#define PROXY_HALT_L    8

/* Query */
#define QUERY_BUFFER_SZ 1024 // 1KB = 4096 bytes

/* Utility */
#define BUFFER_SZ 1024 // default buffer size

/* Cache */
#define CACHE_SZ 10

/* HTTP */
#define DEFAULT_PORT             "80"
#define DEFAULT_PORT_L           2
#define HEADER_END               "\r\n\r\n"
#define HEADER_END_L             4
#define CRLF                     "\r\n"
#define CRLF_L                   2
#define HTTP_GET                 "GET"
#define GET                      "get"
#define GET_L                    3
#define HTTP_CONNECT             "CONNECT"
#define CONNECT                  "connect"
#define CONNECT_L                7
#define FIELD_SEP                ": "
#define FIELD_SEP_L              2
#define SPACE                    " "
#define SPACE_L                  1
#define COLON                    ":"
#define COLON_L                  1
#define HTTP_VERSION_1_1         "HTTP/1.1"
#define HTTP_VERSION_1_1_L       8
#define CONNECTION_ESTABLISHED   "Connection established"
#define CONNECTION_ESTABLISHED_L 22
#define STATUS_200               "200 OK"
#define STATUS_200_L             sizeof(STATUS_200) - 1
#define STATUS_200CE             "200 Connection established"
#define STATUS_200CE_L           sizeof(STATUS_200CE) - 1
#define STATUS_400               "400 Bad Request"
#define STATUS_400_L             sizeof(STATUS_400) - 1
#define STATUS_404               "404 Not Found"
#define STATUS_404_L             sizeof(HTTP_404) - 1
#define STATUS_500               "500 Internal Server Error"
#define STATUS_500_L             sizeof(HTTP_500) - 1
#define STATUS_501               "501 Not Implemented"
#define STATUS_501_L             sizeof(HTTP_501) - 1

/* HTTP Header Fields */
#define CONTENTLENGTH   "content-length:"
#define CONTENTLENGTH_L 15
#define HTTP_HOST       "Host:"
#define HOST            "host:" /* "host:" appears in localhost:9010 in first line */
#define HOST_L          5
#define CACHECONTROL    "cache-control:"
#define CACHECONTROL_L  14
#define MAXAGE          "max-age="
#define MAXAGE_L        8

/* Indicators */

#define CLOSE_CLIENT     111
#define CLIENT_TIMEDOUT  6
#define ERROR_FAILURE    -1
#define ERROR_CLOSE      -2
#define INVALID_REQUEST  -3
#define INVALID_RESPONSE -4
#define INVALID_HEADER   -5
#define ERROR_CONNECT    -6

#define ERROR_BAD_URL     -5
#define ERROR_BAD_HOST    -6
#define ERROR_BAD_PORT    -7
#define ERROR_BAD_METHOD  -8
#define ERROR_BAD_VERSION -9
#define ERROR_BAD_PATH    -10

#define INVALID_RESPONSE -4
#define CLIENT_EXISTS    -5
#define CLIENT_NOT_FOUND -6
#define CANT_DELIVER     -7
#define PARTIAL_MESSAGE  -8
#define OVERFLOW_MESSAGE -9

/* Limits */
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

/* */
#define CLI_QUERY   0
#define CLI_GET     1
#define CLI_CONNECT 2

#endif /* __PROXYCONFIG_H__ */
