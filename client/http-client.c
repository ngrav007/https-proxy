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

static int sendall(int s, char *buf, size_t len);

/*
 * error - wrapper for perror
 */
void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char **argv)
{
    int sockfd, portno, n;
    struct sockaddr_in servaddr;
    struct hostent *server;
    char *hostname;

    /* check command line arguments */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }

    hostname = argv[1];
    portno   = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("[!]] socket: failed\n");
    }
        

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "%s[!]%s unknown host: %s\n", RED, reset, hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&servaddr.sin_addr.s_addr,
          server->h_length);
    servaddr.sin_port = htons(portno);

    /* connect: create a connection with the server */
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        error("[!] connect: failed\n");
    }

    char method[BUFSIZE + 1];
    char url[BUFSIZE + 1];
    char host[BUFSIZE + 1];
    char port[BUFSIZE + 1];
    char body[BUFSIZE + 1];
    char query;
    char command;

    char *raw;
    size_t raw_len;

    bool sentinel = true;
query:
    while (sentinel) {
        /* reset */
        query   = '\0';
        raw     = NULL;
        raw_len = 0;
        command = ' ';
        n       = 0;
        zero(method, BUFSIZE);
        zero(url, BUFSIZE);
        zero(host, BUFSIZE);
        zero(port, BUFSIZE);
        zero(body, BUFSIZE);

        /* Command Prompt */
        printf("[COMMANDS]\n");
        printf("  (1) GET\n");
        printf("  (2) CONNECT\n");
        printf("  (3) HALT\n");

        printf("Command: ");
        scanf(" %c", &command);

        /* Execute Command */
        switch (command) {
        case '1':
            printf("[HTTP GET]\n");
            memcpy(method, HTTP_GET, GET_L);
            printf("URL: ");
            scanf(" %s", url);
            printf("Add Host field? (y/n): ");
            scanf(" %c", &query);
            if (query == 'y') {
                /* populate host with path uri */
                char *host_start = strchr(url, '/') + 2; // skip the "//"
                char *host_end = strchr(host_start, '/');
                if (host_end == NULL) {
                    host_end = url + strlen(url);
                }
                
                memcpy(host, host_start, host_end - host_start);
            }

            raw = Raw_request(method, url, host, port, body, &raw_len);
            n   = sendall(sockfd, raw, raw_len);
            if (n < 0) {
                error("[!] client: error occurred when sending.");
                free(raw);
                return EXIT_FAILURE;
            }
            free(raw);
            break;
        case '2':
            printf("[HTTP CONNECT]\n");
            memcpy(method, HTTP_CONNECT, CONNECT_L);
            printf("Host: ");
            scanf(" %s", host);
            printf("Add Port field? (y/n): ");
            scanf(" %c", &query);
            if (query == 'y') {
                printf("Port: ");
                scanf(" %s", port);
            }

            raw = Raw_request(method, url, host, port, body, &raw_len);
            n   = sendall(sockfd, raw, raw_len);
            if (n < 0) {
                error("[!] client: error occurred when sending.");
                free(raw);
                return EXIT_FAILURE;
            }
            free(raw);
            break;
        case '3':
            printf("[HALT]\n");
            memcpy(method, PROXY_HALT, PROXY_HALT_L);
            memcpy(url, PROXY_HALT, PROXY_HALT_L);
            raw = Raw_request(method, url, host, port, body, &raw_len);
            n   = sendall(sockfd, raw, raw_len);
            if (n < 0) {
                error("[!] client: error occurred when sending.");
                free(raw);
                return EXIT_FAILURE;
            }
            free(raw);
            sentinel = false;
            break;
        default:
            printf("[!] Invalid command! Please try again.\n");
            goto query;
        }

        /* receive reply */
        char *response       = calloc(BUFSIZE + 1, sizeof(char));
        size_t response_len  = 0;
        size_t response_size = BUFSIZE;

        while ((n = recv(sockfd, response + response_len,
                         response_size - response_len, 0)) > 0)
        {
            fprintf(stderr, "[*] Received %d bytes.\r", n);
            response_len += n;
            if ((size_t)n < (response_size - response_len)) {
                response_len += n;
                fprintf(stderr, "[+] Received %d bytes of %ld bytes.\r", n,
                        (response_size - response_len));
                break;
            }

            

            if (response_len >= response_size) {
                response_size *= 2 + 1;
                response = realloc(response, response_size);
            }
        }

        if (n < 0) {
            error("[!] client: error occurred when receiving.");
            free(response);
            close(sockfd);
            return EXIT_FAILURE;
        } else if (n == 0) {
            fprintf(stderr, "[!] client: server closed connection.\n");
            print_ascii(response, response_len);
            fprintf(stderr, "[DEBUG] response_len: %ld\n", response_len);
            sentinel = false;
        } else {
            fprintf(stderr, "[+] Received full response:\n");
            print_ascii(response, response_len);
        }
        free(response);
    }

    close(sockfd);
    return 0;
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
