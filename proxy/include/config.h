#ifndef __PROXYCONFIG_H__
#define __PROXYCONFIG_H__

#include <limits.h>
// #include <fcntl.h> // oflags

/* Compiler Directives */
#define DEBUG      0
#define RUN_SSL    0
#define RUN_CACHE  1
#define RUN_FILTER 1

#if RUN_CACHE != 1      // if cache is disables RUN_COLOR must be disabled
#define RUN_COLOR 1    // define RUN_COLOR here
#else
#define RUN_COLOR 0    
#endif 

/* CA Certificates */
#define CA_CERT_DIR "/workspaces/Development/https-proxy/etc/certs"
#define CA_CERT_L   sizeof(CA_CERT_DIR) - 1

/* Root CA */
#define ROOTCA_CERT        "/workspaces/Development/https-proxy/etc/certs/EnnorRootCA.pem"
#define ROOTCA_KEY         "/workspaces/Development/https-proxy/etc/private/EnnorRootCA.key"
#define ROOTCA_PASSWD_FILE "/workspaces/Development/https-proxy/etc/passwd/EnnorRootCA.passwd"
#define ROOTCA_PASSWD      "friend"

/* Proxy Certificate */
#define PROXY_CERT   "/workspaces/Development/https-proxy/etc/certs/eregion.proxy.crt"
#define PROXY_KEY    "/workspaces/Development/https-proxy/etc/private/eregion.proxy.key"
#define PROXY_CSR    "/workspaces/Development/https-proxy/etc/csr/eregion.proxy.csr"
#define PROXY_EXT    "/workspaces/Development/https-proxy/etc/ext/eregion.proxy.ext"
#define PROXY_PASSWD "friend"

/* Client Certificate */
#define CLIENT_CN     "eregion.client"
#define CLIENT_CERT   "/workspaces/Development/https-proxy/etc/certs/eregion.client.crt"
#define CLIENT_KEY    "/workspaces/Development/https-proxy/etc/private/eregion.client.key"
#define CLIENT_CSR    "/workspaces/Development/https-proxy/etc/csr/eregion.client.csr"
#define CLIENT_EXT    "/workspaces/Development/https-proxy/etc/ext/eregion.client.ext"
#define CLIENT_PASSWD "friend"

/* Script Paths */
#define GENERATE_CERT     "/workspaces/Development/https-proxy/scripts/generate_cert.sh"
#define UPDATE_PROXY_CERT "/workspaces/Development/https-proxy/scripts/update_proxy_cert.sh"
/* Proxy */
#define DEFAULT_MAX_AGE   3600
#define LISTEN_BACKLOG    10
#define TIMEOUT_THRESHOLD 300 // 5 minutes

/* Proxy Halt Signal */
#define HALT         666 // Halt message
#define PROXY_HALT   "__halt__"
#define PROXY_HALT_L 8

/* Query */
#define QUERY_BUFFER_SZ 1024 // 1KB = 4096 bytes

/* Utility */
#define BUFFER_SZ 1024 // default buffer size

/* Proxy Cache */
#define CACHE_SZ 10

/* HTTP ----------------------------------------------------------------------------------------- */
#define HTTP_VERSION_1_1   "HTTP/1.1"
#define HTTP_VERSION_1_1_L 8
#define HTTP               "http://"
#define HTTP_L             7
#define HTTPS              "https://"
#define HTTPS_L            8
#define HTTP_GET           "GET"
#define HTTP_CONNECT       "CONNECT"
#define HTTP_HOST          "Host:"

#define DEFAULT_HTTPS_PORT   "443"
#define DEFAULT_HTTPS_PORT_L 3
#define DEFAULT_HTTP_PORT    "80"
#define DEFAULT_HTTP_PORT_L  2

/* HTTP Errors ---------------------------------------------------------------------------------- */
#define BAD_REQUEST_400           400
#define NOT_FOUND_404             404
#define IM_A_TEAPOT_418           418
#define INTERNAL_SERVER_ERROR_500 500
#define NOT_IMPLEMENTED_501       501
#define BAD_GATEWAY_502           502
#define METHOD_NOT_ALLOWED_405    405
#define PROXY_AUTH_REQUIRED_407   407
#define FORBIDDEN_403             403

#define STATUS_200     "HTTP/1.1 200 OK\r\n\r\n"
#define STATUS_200_L   sizeof(STATUS_200) - 1
#define STATUS_200CE   "HTTP/1.1 200 Connection established\r\n\r\n"
#define STATUS_200CE_L sizeof(STATUS_200CE) - 1
#define STATUS_400     "HTTP/1.1 400 Bad Request\r\n\r\n"
#define STATUS_400_L   sizeof(STATUS_400) - 1
#define STATUS_403     "HTTP/1.1 403 Forbidden\r\n\r\n"
#define STATUS_403_L   sizeof(STATUS_403) - 1
#define STATUS_404     "HTTP/1.1 404 Not Found\r\n\r\n"
#define STATUS_404_L   sizeof(STATUS_404) - 1
#define STATUS_405     "HTTP/1.1 405 Method Not Allowed\r\n\r\n"
#define STATUS_405_L   sizeof(STATUS_405) - 1
#define STATUS_407     "HTTP/1.1 407 Proxy Authentication Required\r\n\r\n"
#define STATUS_407_L   sizeof(STATUS_407) - 1
#define STATUS_418     "HTTP/1.1 418 I'm a teapot\r\n\r\n"
#define STATUS_418_L   sizeof(STATUS_418) - 1
#define STATUS_500     "HTTP/1.1 500 Internal Server Error\r\n\r\n"
#define STATUS_500_L   sizeof(STATUS_500) - 1
#define STATUS_501     "HTTP/1.1 501 Not Implemented\r\n\r\n"
#define STATUS_501_L   sizeof(STATUS_501) - 1
#define STATUS_502     "HTTP/1.1 502 Bad Gateway\r\n\r\n"
#define STATUS_502_L   sizeof(STATUS_502) - 1

/* HTTP Header Fields --------------------------------------------------------------------------- */
#define HEADER_END      "\r\n\r\n"
#define HEADER_END_L    4
#define CRLF            "\r\n"
#define CRLF_L          2
#define GET             "get"
#define GET_L           3
#define CONNECT         "connect"
#define CONNECT_L       7
#define POST            "post"
#define POST_L          4
#define FIELD_SEP       ": "
#define FIELD_SEP_L     2
#define SPACE           " "
#define SPACE_L         1
#define COLON           ":"
#define COLON_L         1
#define CONTENTLENGTH   "content-length:"
#define CONTENTLENGTH_L 15
#define HOST            "host:" /* "host:" appears in localhost:9010 in first line */
#define HOST_L          5
#define CACHECONTROL    "cache-control:"
#define CACHECONTROL_L  14
#define MAXAGE          "max-age="
#define MAXAGE_L        8

/* Event Indicators */
#define SERVER_CLOSE      4
#define CLIENT_CLOSE      3
#define CLIENT_TIMEOUT    2
#define SERVER_RESP_RECVD 1
#define CLIENT_REQ_RECVD  0

#define FILTER_LIST_PATH "/workspaces/Development/https-proxy/proxy/config/filter_list.txt"
#define MAX_FILTERS      100

/* Error Indicators */
#define ERROR_FAILURE       -1
#define ERROR_CLOSE         -2
#define ERROR_SELECT        -3
#define ERROR_ACCEPT        -4
#define ERROR_CONNECT       -5
#define ERROR_FETCH         -6
#define ERROR_SEND          -7
#define ERROR_RECV          -8
#define ERROR_SSL           -9
#define INVALID_REQUEST     -10
#define INVALID_HEADER      -12
#define INVALID_RESPONSE    -11
#define PROXY_AUTH_REQUIRED -13
#define ERROR_BAD_GATEWAY   -14
#define HOST_UNKNOWN        -15
#define ERROR_FILTERED      -16
#define INVALID_URL         -17
#define ERROR_BAD_PORT      -18
#define ERROR_BAD_METHOD    -19
#define ERROR_BAD_VERSION   -20
#define ERROR_BAD_PATH      -21
#define CLIENT_EXISTS       -22
#define CLIENT_NOT_FOUND    -23
#define CANT_DELIVER        -24
#define PARTIAL_MESSAGE     -25
#define OVERFLOW_MESSAGE    -26
#define FILTER_LIST_TOO_BIG -27

/* Limits */
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

/* HTML Parse */
#define ANCHOR_OPEN         "<a "
#define ANCHOR_OPEN_L       3
#define ANCHOR_HTTPS_OPEN   "<a href=\"http"
#define ANCHOR_HTTPS_OPEN_L 13

/* HTML Re-Coloring */
#define RED_F       0
#define GREEN_F     1
#define RED_STYLE   "style=\"color:#FF0000;\" "
#define GREEN_STYLE "style=\"color:#00FF00;\" "
#define COLOR_L     23

/* Client States */
#define CLI_INIT    0
#define CLI_QUERY   1
#define CLI_GET     2
#define CLI_CONNECT 3
#define CLI_SSL     4
#define CLI_POST    5
#define CLI_TUNNEL  6
#define CLI_CLOSE   7

/* Query State */
#define QRY_INIT           0
#define QRY_SENT_REQUEST   1
#define QRY_RECVD_RESPONSE 2
#define QRY_DONE           3
#define QRY_TUNNEL         4

#endif /* __PROXYCONFIG_H__ */
