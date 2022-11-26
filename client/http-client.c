/*
 * http-client.c - A simple TCP client that sends HTTP requests to a server
 *   usage: client <host> <port>
 */

#include "colors.h"
#include "config.h"
#include "http.h"
#include "utility.h"

#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFSIZE 1024
#define OUTPUT_DIR "output"
#define OUTPUT_FILE "response"

static int sendall(int s, char *buf, size_t len);
static int recvall(int s, char **buf, size_t *buf_l, size_t *buf_sz);
static void error(char *msg);
static int connect_to_proxy(char *host, int port);

int main(int argc, char **argv)
{
    /* check command line arguments */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <proxy-host> <proxy-port> <method> <host> [uri]\n", argv[0]);
        exit(0);
    }

    /* get proxy host and port */
    char *proxy_host = argv[1];
    int proxy_port = atoi(argv[2]);

    /* get method, uri, and host */
    char *method = argv[3];
    char *host = argv[4];
    char *uri = NULL;
    if (argc == 6) {
        uri = argv[5];
    } else {
        uri = "/";
    }

    /* connect to proxy */
    int proxy_fd = connect_to_proxy(proxy_host, proxy_port);
    if (proxy_fd < 0) {
        error("[-] http: Failed to connect to proxy");
    }

    /* build raw request */
    size_t raw_l = 0;
    char *raw_request = Raw_request(method, uri, host, NULL, NULL, &raw_l);
    if (raw_request == NULL) {
        error("[-] http: Failed to build raw request");
    }

    /* send raw request */
    if (sendall(proxy_fd, raw_request, raw_l) < 0) {
        error("[-] http: Failed to send raw request");
    }

    /* receive raw response */
    size_t raw_response_l = 0;
    size_t raw_response_sz = 0;
    char *raw_response = NULL;
    if (recvall(proxy_fd, &raw_response, &raw_response_l, &raw_response_sz) < 0) {
        error("[-] http: Failed to receive raw response");
    }

    fprintf(stderr, "[*] http: Raw response:\n");

    print_ascii(raw_response, raw_response_l);

    free(raw_request);
    free(raw_response);
    close(proxy_fd);

    return 0;
}


static int recvall(int s, char **buf, size_t *buf_l, size_t *buf_sz)
{
    if (buf == NULL || buf_l == NULL || buf_sz == NULL) {
        return EXIT_FAILURE;
    }
    
    /* create buffer if it doesn't exist */
    if (*buf == NULL) {
        *buf = calloc(BUFSIZE + 1, sizeof(char));
        if (*buf == NULL) {
            return EXIT_FAILURE;
        }
        *buf_sz = BUFSIZE;
        *buf_l = 0;
    } else {
        /* if buffer exists, make sure it's big enough */
        if (*buf_sz < BUFSIZE) {
            *buf = realloc(*buf, BUFSIZE + 1);
            if (*buf == NULL) {
                return EXIT_FAILURE;
            }
            *buf_sz = BUFSIZE;
        }
    }
 
    /* receive data */
    fprintf(stderr, "*buf_l: %ld, *buf_sz: %ld\n", *buf_l, *buf_sz);
    ssize_t n = 0, ret = 0;
    while (1) {
        n = recv(s, *buf + *buf_l, *buf_sz - *buf_l, 0);
        if (n < 0) {
            error("[!] client: error occurred when receiving.");
            return EXIT_FAILURE;
        } else if (n == 0 || (size_t)n < (*buf_sz - *buf_l)) {
            *buf_l += n;
            (*buf)[*buf_l] = '\0';
            break;
        }
        *buf_l += n;
        if (*buf_l >= *buf_sz) {
            ret = expand_buffer(buf, buf_l, buf_sz);
            if (ret < 0) {
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}


static int sendall(int s, char *buf, size_t len)
{
    size_t total     = 0;   // how many bytes we've sent
    size_t bytes_left = len; // how many we have left to send
    int n;

    while (total < len) {
        n = send(s, buf + total, bytes_left, 0);
        if (n == -1) {
            break;
        }
        total += n;
        bytes_left -= n;
    }

    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}

/*
 * error - wrapper for perror
 */
static void error(char *msg)
{
    perror(msg);
    exit(0);
}

static int connect_to_proxy(char *host, int port)
{
    struct sockaddr_in servaddr;
    struct hostent *server;

    /* socket: create the socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("[!] socket: ");
    }

    fprintf(stderr, "[+] Connecting to proxy %s:%d\n", host, port);
        
    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(host);
    if (server == NULL) {
        error("[!] gethostbyname: ");
    }

    /* build the server's Internet address */
    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&servaddr.sin_addr.s_addr, server->h_length);
    servaddr.sin_port = htons(port);

    /* connect: create a connection with the server */
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        error("[!] connect: ");
    }

    return sockfd;
}
