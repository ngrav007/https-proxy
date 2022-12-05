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

/* HTTP ---------------------------------------------------------------------- */
#define BAD_REQUEST_400           400
#define NOT_FOUND_404             404
#define IM_A_TEAPOT_418           418
#define INTERNAL_SERVER_ERROR_500 500
#define NOT_IMPLEMENTED_501       501

#define STATUS_200     "200 OK\r\n\r\n"
#define STATUS_200_L   sizeof(STATUS_200) - 1
#define STATUS_200CE   "200 Connection established\r\n\r\n"
#define STATUS_200CE_L sizeof(STATUS_200CE) - 1
#define STATUS_400     "400 Bad Request\r\n\r\n"
#define STATUS_400_L   sizeof(STATUS_400) - 1
#define STATUS_404     "404 Not Found\r\n\r\n"
#define STATUS_404_L   sizeof(STATUS_404) - 1
#define STATUS_418     "418 I'm a teapot\r\n\r\n"
#define STATUS_418_L   sizeof(STATUS_418) - 1
#define STATUS_500     "500 Internal Server Error\r\n\r\n"
#define STATUS_500_L   sizeof(STATUS_500) - 1
#define STATUS_501     "501 Not Implemented\r\n\r\n"
#define STATUS_501_L   sizeof(STATUS_501) - 1
#define STATUS_502     "502 Bad Gateway\r\n\r\n"
#define STATUS_502_L    sizeof(STATUS_502) - 1

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

/* Error and Event Indicators */

#define CLIENT_CLOSE    100
#define CLIENT_TIMEDOUT 101

#define ERROR_FAILURE -1
#define ERROR_CLOSE   -2
#define ERROR_SELECT  -3
#define ERROR_ACCEPT  -4
#define ERROR_CONNECT -5
#define ERROR_FETCH   -6
#define ERROR_SEND    -7
#define ERROR_RECV    -8
#define ERROR_SSL     -9
#define INVALID_REQUEST  -10
#define INVALID_RESPONSE -11



#define INVALID_HEADER   -21

#define ERROR_BAD_URL     -5
#define HOST_INVALID      -6
#define HOST_UNKNOWN      -11
#define ERROR_BAD_PORT    -7
#define ERROR_BAD_METHOD  -8
#define ERROR_BAD_VERSION -9
#define ERROR_BAD_PATH    -10

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
#define CLI_SSL     3

/* */
#define ANCHOR_OPEN "<a "
#define ANCHOR_OPEN_L 3
#define ANCHOR_HTTPS_OPEN "<a href=\"https"
#define ANCHOR_HTTPS_OPEN_L 14 

/* */
#define RED_F 0
#define GREEN_F 1
#define RED_STYLE "style=\"color:#FF0000;\" "
#define GREEN_STYLE "style=\"color:#00FF00;\" "
#define COLOR_L 23

/* SSL */
#define CERT_FILE "/workspaces/Development/etc/certs/server.pem"
#define KEY_FILE  "/workspaces/Development/etc/certs/server.key"

#endif /* __PROXYCONFIG_H__ */
