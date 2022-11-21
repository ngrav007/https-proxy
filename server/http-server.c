
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

int main(int argc, char **argv)
{
    int sockfd, portno, n;
    struct sockaddr_in servaddr;
    struct hostent *server;
    char *hostname;

    /* check command line arguments */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    hostname = argv[1];
    portno   = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "[!] socket: failed\n");
        return EXIT_FAILURE;
    }

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "[!] unknown host: %s\n", hostname);
        close(sockfd);
        return EXIT_FAILURE;
    }

    /* build the server's Internet address */
    bzero((char *) &servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&servaddr.sin_addr.s_addr, server->h_length);
    servaddr.sin_port = htons(portno);

    /* bind: associate the parent socket with a port */
    if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "[!] bind: failed\n");
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
        clientfd = accept(sockfd, (struct sockaddr *) &clientaddr, &clientlen);
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

        response = Raw_request(request->method, request->path, request->host, request->port, request->body, &response_len);
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

        /* close: close the connection */
        close(clientfd);
        Request_free(request);
        free(response);
        break;
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
