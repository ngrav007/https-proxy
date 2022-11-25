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
static void error(char *msg);
static int connect_to_proxy(char *host, int port);

int main(int argc, char **argv)
{
    /* check command line arguments */
    if (argc < 4 ) {
        fprintf(stderr, "Usage: %s <proxy-host> <proxy-port> <method> <host> [port]\n", argv[0]);
        exit(0);
    }

    /* connect to proxy */
    char *proxy_host = argv[1];
    int proxy_port = atoi(argv[2]);
    int proxyfd = connect_to_proxy(proxy_host, proxy_port);
    if (proxyfd < 0) {
        error("connect_to_proxy: failed\n");
    }

    /* create raw request */
    size_t raw_len = 0;
    char *raw = NULL;
    char *method = argv[3];
    char *host = argv[4];
    char *body = "";
    char *port;
    if (argc == 6) {
        port = argv[5];
    } else {
        port = (strncmp(method, GET, GET_L) == 0 || strncmp(method, "GET", GET_L) == 0) ? "80" : "443";
    }
    char url[BUFSIZE + 1];
    zero(url, BUFSIZE + 1);
    snprintf(url, BUFSIZE, "%s:%s", host, port);
    raw = Raw_request(method, url, host, port, body, &raw_len);
    if (raw == NULL) {
        error("Raw_request: failed\n");
    }

    /* add connection closed field */
    HTTP_add_field(&raw, "Connection", "close", &raw_len);

    /* send request */
    int n = sendall(proxyfd, raw, raw_len);
    if (n < 0) {
        error("sendall: failed\n");
    }
    free(raw);

    /* receive response */
    char *response       = calloc(BUFSIZE + 1, sizeof(char));
    size_t response_len  = 0;
    size_t response_size = BUFSIZE;
    while (1) {
        n = recv(proxyfd, response + response_len, response_size - response_len, 0);
        if (n < 0) {
            error("[!] client: error occurred when receiving.");
            free(response);
            return EXIT_FAILURE;
        } else if (n == 0 || (size_t)n < (response_size - response_len)) {
            response[response_len] = '\0';
            break;
        }
        response_len += n;
        if (response_len >= response_size) {
            response_size *= 2;
            response = realloc(response, response_size);
        }
    }

    fprintf(stderr, "[+] Received full response (%ld):\n", response_len);
    print_ascii(response, response_len);
        
    /* write to file to output directory */
    char filename[BUFSIZE + 1]; 
    zero(filename, BUFSIZE + 1);
    sprintf(filename, "%s/%s-%s-%ld.txt", OUTPUT_DIR, OUTPUT_FILE, host, time(NULL));
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        error("[!] client: failed to open file.");
        free(response);
        close(proxyfd);
        return EXIT_FAILURE;
    }

    /* write full response to file */
    fprintf(fp, "%s", response);
    fclose(fp);
        
    free(response);
    close(proxyfd);

    return EXIT_SUCCESS;
}


static int sendall(int s, char *buf, size_t len)
{
    size_t total     = 0;   // how many bytes we've sent
    size_t bytesleft = len; // how many we have left to send
    int n;

    while (total < len) {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1) {
            break;
        }
        total += n;
        bytesleft -= n;
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
        error("[!] socket: failed");
    }
        
    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(host);
    if (server == NULL) {
        error("[!] gethostbyname: failed");
    }

    /* build the server's Internet address */
    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&servaddr.sin_addr.s_addr, server->h_length);
    servaddr.sin_port = htons(port);

    /* connect: create a connection with the server */
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        error("[!] connect: failed");
    }

    return sockfd;
}
