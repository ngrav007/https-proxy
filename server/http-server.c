
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

#define BUFSIZE 4096

static int sendall(int s, char *buffer, size_t len);
char *get_response(char *version, char *status, char *body, size_t *len);

int main(int argc, char **argv)
{
    int sockfd, portno, n;
    struct sockaddr_in servaddr;

    /* check command line arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    portno   = atoi(argv[1]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "[!] socket: failed\n");
        return EXIT_FAILURE;
    }

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
                   sizeof(optval)) == -1)
    {
        fprintf(stderr, "[!] setsockopt: failed\n");
        close(sockfd);
        return ERROR_FAILURE;
    }

    /* build the server's Internet address */
    bzero((char *) &servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons((unsigned short)portno);


    /* bind: associate the parent socket with a port */
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "[!] bind: failed\n");
        perror("bind");
        close(sockfd);
        return EXIT_FAILURE;
    }

    /* listen: make this socket ready to accept connection requests */
    if (listen(sockfd, 5) < 0) {
        fprintf(stderr, "[!] listen: failed\n");
        close(sockfd);
        return EXIT_FAILURE;
    }

    /* main loop: wait for a connection request, echo input line,
       then close connection. */

    while (true) {
        int clientfd;
        struct sockaddr_in clientaddr;
        socklen_t clientlen = sizeof(clientaddr);
        char buffer[BUFSIZE];
        Request *request;
        char *response;
        size_t request_len;
        size_t response_len;
        int status;

        /* accept: wait for a connection request */
        clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
        if (clientfd < 0) {
            fprintf(stderr, "[!] accept: failed\n");
            close(sockfd);
            return EXIT_FAILURE;
        }

        /* read: read input string from the client */
        bzero(buffer, BUFSIZE);
        request_len = 0;
        while ((n = read(clientfd, buffer, BUFSIZE)) > 0) {
            if (n < 0) {
                fprintf(stderr, "[!] read: failed\n");
                close(clientfd);
                close(sockfd);
                return EXIT_FAILURE;
            }
            request_len += n;
            if (request_len >= BUFSIZE) {
                fprintf(stderr, "[!] request too long\n");
                close(clientfd);
                break;
            }

            if (strstr(buffer, "\r\n\r\n") != NULL) {
                break;
            }
        }

        /* parse request */
        request = Request_new(buffer, request_len);
        if (request == NULL) {
            fprintf(stderr, "[!] invalid request\n");
            close(clientfd);
            continue;
        }

        /* parse uri */
        Request_print(request);

        /* identify the file to be sent */
        if (strncmp(request->method, GET, GET_L) == 0) {
            char *filename = strrchr(request->path, '/');
            if (filename == NULL) {
                filename = request->path;
            } else {
                filename++;
            }

            if (filename[0] == '\0') {
                filename = "index.html";
            }

            FILE *fp = fopen(filename, "r");
            if (fp == NULL) {
                fprintf(stderr, "[!] fopen: failed\n");
                response = get_response(HTTP_VERSION_1_1, STATUS_404, NULL, &response_len);
                if (response == NULL) {
                    fprintf(stderr, "[!] get_response: failed\n");
                    close(clientfd);
                    continue;
                }
            } else {
                char *body = malloc(BUFSIZE * sizeof(char) + 1);
                size_t body_len = 0;
                size_t body_sz = 0;
                char buf[BUFSIZE];
                while ((n = fread(buf, 1, BUFSIZE, fp)) > 0) {
                    if (n < 0) {
                        fprintf(stderr, "[!] fread: failed\n");
                        fclose(fp);
                        close(clientfd);
                        continue;
                    }

                    if (body_len + n >= body_sz) {
                        body_sz += BUFSIZE;
                        body = realloc(body, body_sz + 1);
                        if (body == NULL) {
                            fprintf(stderr, "[!] realloc: failed\n");
                            fclose(fp);
                            close(clientfd);
                            continue;
                        }
                    }

                    memcpy(body + body_len, buf, n);
                    body_len += n;
                    body[body_len] = '\0';
                }
                fclose(fp);

                response = get_response(HTTP_VERSION_1_1, STATUS_200, body, &response_len);
                if (response == NULL) {
                    fprintf(stderr, "[!] get_response: failed\n");
                    close(clientfd);
                    continue;
                }

                free(body);
            }
            
            status = sendall(clientfd, response, response_len);
            if (status == -1) {
                fprintf(stderr, "[!] sendall: failed\n");
                close(clientfd);
                close(sockfd);
                return EXIT_FAILURE;
            }
        } else {
             response = Raw_request(request->method, request->path, request->host,
                               request->port, request->body, &response_len);
            if (response == NULL) {
                fprintf(stderr, "[!] invalid response\n");
                close(clientfd);
                close(sockfd);
                return EXIT_FAILURE;
            }
            /* write: echo the input string back to the client */
            status = sendall(clientfd, response, response_len);
            if (status < 0) {
                fprintf(stderr, "[!] sendall: failed\n");
                close(clientfd);
                close(sockfd);
                return EXIT_FAILURE;
            }
        }

       

        /* close: close the connection */
        close(clientfd);
        Request_free(request);
        free(response);
    }

    close(sockfd);

    return 0;
}

static int sendall(int s, char *buffer, size_t len)
{
    size_t total     = 0;   // how many bytes we've sent
    size_t bytesleft = len; // how many we have left to send
    int n;

    while (total < len) {
        n = send(s, buffer + total, bytesleft, 0);
        if (n == -1) {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}

char *get_response(char *version, char *status, char *body, size_t *len)
{
    if (version == NULL || status == NULL) {
        return NULL;
    }
    char *response;
    size_t response_len, version_len, status_len, body_len;

    version_len = strlen(version);
    status_len = strlen(status);
    if (body == NULL) {
        body_len = 0;
    } else {
        body_len = strlen(body);
    }

    response_len = version_len + status_len + body_len + CRLF_L + CRLF_L;

    response = calloc(response_len + 1, sizeof(char));
    if (response == NULL) {
        return NULL;
    }

    int offset = 0;
    memcpy(response, version, version_len);
    offset += version_len;
    memcpy(response + offset, SPACE, SPACE_L);
    offset += SPACE_L;
    memcpy(response + offset, status, status_len);
    offset += status_len;
    memcpy(response + offset, CRLF, CRLF_L);
    offset += CRLF_L;
    if (body_len > 0) {
        memcpy(response + offset, body, body_len);
        offset += body_len;
    }
    memcpy(response + offset, CRLF, CRLF_L);
    offset += CRLF_L;

    *len = response_len;

    return response;
}
