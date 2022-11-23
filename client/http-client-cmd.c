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
static void error(char *msg);

int main(int argc, char **argv)
{
    int sockfd, proxy_port, request_host, n;
    struct sockaddr_in servaddr;
    struct hostent *server;
    char *proxy_host, *request_host, *method, *body;

    /* check command line arguments */
    if (argc < 5) {
        fprintf(stderr, "Usage: %s proxy-host proxy-port method host [port] [body]\n", argv[0]);
        exit(0);
    }

    /* parse command line arguments */
    proxy_host = argv[1];
    proxy_port = atoi(argv[2]);
    method = argv[3];
    request_host = argv[4];
    request_port = (argc == 6) ? atoi(argv[5]) : DEFAULT_PORT;
    body = (argc == 7) ? argv[6] : NULL;

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("[!]] socket: failed");
    }
        
    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(proxy_host);
    if (server == NULL) {
        fprintf(stderr, "%s[!]%s unknown host: %s\n", RED, reset, proxy_host);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&servaddr.sin_addr.s_addr,
          server->h_length);
    servaddr.sin_port = htons(proxy_port);

    /* connect: create a connection with the server */
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        error("[!] connect: failed");
    }

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
        zero(method, BUFSIZE + 1);
        zero(url, BUFSIZE + 1);
        zero(host, BUFSIZE + 1);
        zero(port, BUFSIZE + 1);
        zero(body, BUFSIZE + 1);

        /* Command Prompt */
        printf("[COMMANDS]\n");
        printf("  (1) GET\n");
        printf("  (2) CONNECT\n");
        printf("  (3) HALT\n");

        printf("Command: ");
        scanf(" %c", &command);
        fflush(stdin);

        /* Execute Command */
        switch (command) {
        case '1':
            printf("[HTTP GET]\n");
            memcpy(method, HTTP_GET, GET_L);
            printf("URL: ");
            scanf(" %s", url);
            printf("Add Host field? (y/n): ");
            scanf(" %c", &query);
            fflush(stdin);
            if (query == 'y') {
                /* populate host with path uri */
                char *host_start = strstr(url, "//");
                if (host_start == NULL) {
                    fprintf(stderr, "%s[!]%s invalid url: %s\n", RED, reset,
                            url);
                    goto query;
                }
                host_start += 2;
                char *host_end = strchr(host_start, '/');
                if (host_end == NULL) {
                    host_end = url + strlen(url);
                }

                
                
                memcpy(host, host_start, host_end - host_start);
                fprintf(stderr, "%s[!]%s host: %s\n", RED, reset, host);
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
            printf("URL: ");
            scanf(" %s", url);
            printf("Add Host field? (y/n): ");
            scanf(" %c", &query);
            fflush(stdin);
            if (query == 'y') {
                /* populate host with path uri */
                char *host_start = strstr(url, "//");
                if (host_start == NULL) {
                    fprintf(stderr, "%s[!]%s invalid url: %s\n", RED, reset,
                            url);
                    goto query;
                }
                host_start += 2;
                char *host_end = strchr(host_start, '/');
                if (host_end == NULL) {
                    host_end = url + strlen(url);
                }

                /* add port */
                memcpy(host, host_start, host_end - host_start);
                memcpy(port, "443", 3);
                fprintf(stderr, "%s[!]%s host: %s\n", RED, reset, host);
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
            if ((size_t)n < (response_size - response_len)) {
                response_len += n;
                fprintf(stderr, "[+] Received %d bytes of %ld bytes.\r", n,
                        (response_size - response_len));
                break;
            }
            response_len += n;

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
            sentinel = false;
        } else {
            fprintf(stderr, "[+] Received full response:\n");
            print_ascii(response, response_len);
        }
        /* write to file */
        char filename[BUFSIZE + 1];
        zero(filename, BUFSIZE + 1);
        printf("Save response to file? (y/n): ");
        scanf(" %c", &query);
        fflush(stdin);
        

        if (query == 'y') {
            printf("Filename: ");
            scanf(" %s", filename);
            fflush(stdin);
            FILE *fp = fopen(filename, "w");
            if (fp == NULL) {
                error("[!] client: failed to open file.");
                free(response);
                close(sockfd);
                return EXIT_FAILURE;
            }
            
            char *body = strstr(response, "\r\n\r\n");
            if (body == NULL) {
                error("[!] client: failed to find body.");
                free(response);
                close(sockfd);
                return EXIT_FAILURE;
            }
            body += 4;
            size_t body_len = response_len - (body - response);
            if (fwrite(body, sizeof(char), body_len, fp) != body_len) {
                error("[!] client: failed to write to file.");
                free(response);
                close(sockfd);
                return EXIT_FAILURE;
            }
            fclose(fp);
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

/*
 * error - wrapper for perror
 */
static void error(char *msg)
{
    perror(msg);
    exit(0);
}
