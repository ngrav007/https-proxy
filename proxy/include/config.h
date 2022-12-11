#ifndef __PROXYCONFIG_H__
#define __PROXYCONFIG_H__

#include <limits.h>
// #include <fcntl.h> // oflags

#define RUN_CACHE   1
#define RUN_SSL     0
#define RUN_COLOR   0 


#define DEBUG 1
#define TRUE  1
#define FALSE 0

/* Root CA */
#define ROOTCA_CERT "/workspaces/Development/https-proxy/etc/certs/EnnorRootCA.pem"
#define ROOTCA_KEY "/workspaces/Development/https-proxy/etc/private/EnnorRootCA.key"
#define ROOTCA_PASSWD_FILE "/workspaces/Development/https-proxy/etc/passwd/EnnorRootCA.passwd"
#define ROOTCA_PASSWD "friend"

/* Proxy Certificate */
#define PROXY_CERT "/workspaces/Development/https-proxy/etc/certs/eregion.proxy.crt"
#define PROXY_KEY "/workspaces/Development/https-proxy/etc/private/eregion.proxy.key"
#define PROXY_CSR "/workspaces/Development/https-proxy/etc/csr/eregion.proxy.csr"
#define PROXY_EXT "/workspaces/Development/https-proxy/etc/ext/eregion.proxy.ext"
#define PROXY_PASSWD "friend"

/* Client Certificate */
#define CLIENT_CN "eregion.client"
#define CLIENT_CERT "/workspaces/Development/https-proxy/etc/certs/eregion.client.crt"
#define CLIENT_KEY "/workspaces/Development/https-proxy/etc/private/eregion.client.key"
#define CLIENT_CSR "/workspaces/Development/https-proxy/etc/csr/eregion.client.csr"
#define CLIENT_EXT "/workspaces/Development/https-proxy/etc/ext/eregion.client.ext"
#define CLIENT_PASSWD "friend"

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
#define GET                      "get"
#define GET_L                    3
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
#define HTTP                      "http://"
#define HTTP_L                    7
#define HTTPS                     "https://"
#define HTTPS_L                   8
#define HTTP_GET                 "GET"
#define HTTP_CONNECT             "CONNECT"


/* HTTP ---------------------------------------------------------------------- */

#define BAD_REQUEST_400           400
#define NOT_FOUND_404             404
#define IM_A_TEAPOT_418           418
#define INTERNAL_SERVER_ERROR_500 500
#define NOT_IMPLEMENTED_501       501
#define PROXY_AUTH_REQUIRED_407   407

#define STATUS_200     "200 OK\r\n\r\n"
#define STATUS_200_L   sizeof(STATUS_200) - 1
#define STATUS_200CE   "200 Connection established\r\n\r\n"
#define STATUS_200CE_L sizeof(STATUS_200CE) - 1
#define STATUS_400     "400 Bad Request\r\n\r\n"
#define STATUS_400_L   sizeof(STATUS_400) - 1
#define STATUS_404     "404 Not Found\r\n\r\n"
#define STATUS_404_L   sizeof(STATUS_404) - 1
#define STATUS_407     "407 Proxy Authentication Required\r\n\r\n"
#define STATUS_407_L   sizeof(STATUS_407) - 1
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

#define CLIENT_CLOSE    -100
#define CLIENT_TIMEDOUT -101

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
#define PROXY_AUTH_REQUIRED   -13


#define INVALID_HEADER   -12
#define INVALID_HOST      -6

#define INVALID_URL     -5
#define HOST_UNKNOWN      -15
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

/* HTML Parse */
#define ANCHOR_OPEN "<a "
#define ANCHOR_OPEN_L 3
#define ANCHOR_HTTPS_OPEN "<a href=\"http"
#define ANCHOR_HTTPS_OPEN_L 13

/* HTML Re-Coloring */
#define RED_F 0
#define GREEN_F 1
#define RED_STYLE   "style=\"color:#FF0000;\" "
#define GREEN_STYLE "style=\"color:#00FF00;\" "
#define COLOR_L 23

/* Client States */
#define CLI_QUERY   0
#define CLI_GET     1
#define CLI_CONNECT 2
#define CLI_SSL     3
#define CLI_PLAIN   4


#endif /* __PROXYCONFIG_H__ */
