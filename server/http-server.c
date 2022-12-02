
#include "http.h"
#include "utility.h"

#include <fcntl.h>
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
#define FILE_DIR "files/"
#define FILE_DIR_L sizeof(FILE_DIR) - 1

static long sendall(int s, char *request, size_t len);
char *get_response(char *version, char *status, char *body, size_t body_l, size_t *len);

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
        char request[BUFSIZE];
        Request *reqinfo;
        char *response;
        size_t request_len;
        size_t response_len;
        int sendall_ret;

        /* accept: wait for a connection request */
        clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
        if (clientfd < 0) {
            fprintf(stderr, "[!] accept: failed\n");
            close(sockfd);
            return EXIT_FAILURE;
        }

        /* read: read input string from the client */
        bzero(request, BUFSIZE);
        request_len = 0;
        while ((n = read(clientfd, request, BUFSIZE)) > 0) {
            if (n < 0) {
                fprintf(stderr, "[!] read: failed\n");
                close(clientfd);
                close(sockfd);
                return EXIT_FAILURE;
            }
            request_len += n;
            if (request_len >= BUFSIZE) {
                fprintf(stderr, "[!] request too long\n");
                response = get_response(HTTP_VERSION_1_1, STATUS_400, NULL, 0, &response_len);
                sendall_ret = sendall(clientfd, response, response_len);
                free(response);
                close(clientfd);
                break;
            }

            if (n < BUFSIZE) {
                break;
            }
        }

        /* parse request */
        reqinfo = Request_new(request, request_len);
        if (reqinfo == NULL) {
            fprintf(stderr, "[!] invalid request\n");
            close(clientfd);
            continue;
        }

        /* parse uri */
        Request_print(reqinfo);

        /* identify the file to be sent */
        if (strncmp(reqinfo->method, GET, GET_L) == 0) {
            char *basename = strrchr(reqinfo->path, '/');
            if (basename == NULL) {
                basename = reqinfo->path;
            } else {
                basename++;
            }

            if (basename[0] == '\0') {
                basename = "index.html";
            }

            char filename[BUFSIZE];
            bzero(filename, BUFSIZE);
            memcpy(filename, FILE_DIR, FILE_DIR_L);
            memcpy(filename + FILE_DIR_L, basename, strlen(basename));

            fprintf(stderr, "[+] Filename: %s\n", filename);
            
            int fp = open(filename, O_RDONLY);
            if (fp == -1) {
                fprintf(stderr, "[*] %s not found\n", filename);
                response = get_response(HTTP_VERSION_1_1, STATUS_404, NULL, 0, &response_len);
                if (response == NULL) {
                    fprintf(stderr, "[!] get_response: failed\n");
                    close(clientfd);
                    continue;
                }
            } else {
                char *body = malloc(BUFSIZE * sizeof(char) + 1);
                size_t body_len = 0;
                size_t body_sz = BUFSIZE;
                while ((n = read(fp, body + body_len, (body_sz - body_len))) > 0) {
                    if (n < 0) {
                        fprintf(stderr, "[!] read: failed\n");
                        free(body);
                        close(fp);
                        close(clientfd);
                        close(sockfd);
                        return EXIT_FAILURE;
                    }
                    body_len += n;
                    if (body_len >= body_sz) {
                        if (expand_buffer(&body, &body_len, &body_sz) < 0) {
                            fprintf(stderr, "[!] expand_buffer: failed\n");
                            free(body);
                            close(fp);
                            close(clientfd);
                            close(sockfd);
                            return EXIT_FAILURE;
                        }
                    }
                }
                body[body_len] = '\0';
                response = get_response(HTTP_VERSION_1_1, STATUS_200, body, body_len, &response_len);
                if (response == NULL) {
                    fprintf(stderr, "[!] get_response: failed\n");
                    close(clientfd);
                    continue;
                }

                free(body);
                close(fp);
            }
            
            sendall_ret = sendall(clientfd, response, response_len);
            fprintf(stderr, "[+] %d bytes sent\n", sendall_ret);
            if (sendall_ret == -1) {
                fprintf(stderr, "[!] sendall: failed\n");
                free(response);
                close(clientfd);
                close(sockfd);
                return EXIT_FAILURE;
            }
        } else {
            response = get_response(HTTP_VERSION_1_1, STATUS_501, request, request_len, &response_len);
            if (response == NULL) {
                fprintf(stderr, "[!] invalid response\n");
                close(clientfd);
                close(sockfd);
                return EXIT_FAILURE;
            }
            /* write: echo the input string back to the client */
            sendall_ret = sendall(clientfd, response, response_len);
            fprintf(stderr, "[+] %d bytes sent\n", sendall_ret);
            if (sendall_ret < 0) {
                fprintf(stderr, "[!] sendall: failed\n");
                free(response);
                close(clientfd);
                close(sockfd);
                return EXIT_FAILURE;
            }
        }

        /* close: close the connection */
        close(clientfd);
        Request_free(reqinfo);
        free(response);
    }

    close(sockfd);

    return 0;
}

static long sendall(int s, char *request, size_t len)
{
    size_t total     = 0;   // how many bytes we've sent
    size_t bytesleft = len; // how many we have left to send
    int n;

    while (total < len) {
        n = send(s, request + total, bytesleft, 0);
        if (n == -1) {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    return n == -1 ? -1 : (long)total; // return -1 on failure, 0 on success
}

char *get_response(char *version, char *status, char *body, size_t body_l, size_t *len)
{
    if (version == NULL || status == NULL) {
        return NULL;
    }
    char *response;
    size_t response_len, version_len, status_len, body_len;

    version_len = strlen(version);
    status_len = strlen(status);
    if (body != NULL) {
        body_len = body_l;
    } else {
        body_len = 0;
    }

    response_len = version_len + SPACE_L + status_len + CRLF_L + CRLF_L + body_len;

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

    /* start of body */
    memcpy(response + offset, CRLF, CRLF_L);
    offset += CRLF_L;
    if (body_len > 0) {
        memcpy(response + offset, body, body_len);
        offset += body_len;
    }
    
    *len = response_len;

    return response;
}
