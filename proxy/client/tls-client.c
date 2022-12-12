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
#include <fcntl.h>

// #if RUN_SSL
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
// #endif 

#define BUFSIZE 1024
#define OUTPUT_DIR "output"
#define OUTPUT_FILE "resp"
#define SAVE_FILE 0
#define PERMS 0644

static int sendall(int s, char *buf, size_t len);
static int recvall(int s, char **buf, size_t *buf_l, size_t *buf_sz);
static void error(char *msg);
static int connect_to_proxy(char *host, int port);
static int save_to_file(char *uri, char *raw_response, size_t raw_response_l);
static int ssl_readall(SSL *s, char **buf, size_t *buf_l, size_t *buf_sz);


int main(int argc, char **argv)
{
    /* check command line arguments */
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <proxy-host> <proxy-port> <method> <host> [uri]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *raw_response = NULL;
    size_t raw_response_l = 0, bytes_read = 0;
    ssize_t bytes;
    char buffer[BUFSIZE] = {0};
    
    /* get information to form request */
    char *proxy_host = argv[1];
    int proxy_port = atoi(argv[2]);
    char *method = argv[3];
    char *host = argv[4];
    char *uri = (argc == 6) ? argv[5] : "/";
    char port[MAX_DIGITS_LONG] = {0};
    char *port_start = strchr(method, ':');
    if (port_start == NULL) {
        port_start = NULL;
    } else {
        *port_start = '\0';
        port_start++;
        char *port_end = port_start;
        while(isdigit(port_end)) {
            port_end++;
        }        
        memcpy(port, port_start, port_end-port_start);
    }

    /* build raw request */
    size_t raw_l = 0;
    char *raw_request = Raw_request(method, uri, host, port, NULL, &raw_l);
    if (raw_request == NULL) {
        error("[!] Failed to build raw request");
    }

    /* Setup Connection ------------------------------------------------------------------------- */
    int proxy_fd = connect_to_proxy(proxy_host, proxy_port);
    if (proxy_fd < 0) {
        close(proxy_fd);
        error("[!] Failed to connect to proxy");
    }

    /* send raw request */
    fprintf(stderr, "[+] Sending raw request\n");
    if (sendall(proxy_fd, raw_request, raw_l) < 0) {
        close(proxy_fd);
        error("[!] Failed to send raw request");
    }

    /* receive raw response */
    print_info("[+] Receiving raw response");
    if (recvall(proxy_fd, &raw_response, &raw_response_l, &bytes_read) < 0) {
        close(proxy_fd);
        error("[!] Failed to receive raw response");
    }

    
    /* TLS/SSL Handshake ------------------------------------------------------------------------ */
    
    /* SSL Initialize */
    fprintf(stderr, "[+] Initializing SSL Library\n");
    SSL_library_init();

    fprintf(stderr, "[+] Initializing SSL Context\n");
    SSL_CTX *ctx = InitCTX();

    fprintf(stderr, "[+] Loading SSL Certificates\n");
    LoadClientCertificates(ctx, CLIENT_CERT, CLIENT_KEY);

    fprintf(stderr, "[+] Creating SSL Object\n");
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, proxy_fd);

    fprintf(stderr, "[+] SSL Connect\n");
    if (SSL_connect(ssl) == -1) {
        fprintf(stderr, "[!] SSL_connect failed\n");
        ERR_print_errors_fp(stderr);
        close(proxy_fd);
        SSL_free(ssl);
        error("[!] Failed failed on SSL_connect");
    } else {
        printf("[+] Connected with %s encryption\n", SSL_get_cipher(ssl));
        ShowCerts(ssl);
        SSL_write(ssl, raw_request, raw_l);
        while ((bytes = SSL_read(ssl, buffer, BUFSIZE)) > 0) {
            if (raw_response == NULL) {
                raw_response = malloc(BUFSIZE);
                raw_response_l = BUFSIZE;
            } else if (raw_response_l < bytes_read + bytes) {
                raw_response_l += BUFSIZE;
                raw_response = realloc(raw_response, raw_response_l);
            }
            memset(buffer, 0, BUFSIZE);
        }
        if (bytes < 0) {
            fprintf(stderr, "SSL_read failed\n");
            ERR_print_errors_fp(stderr);
            close(proxy_fd);
            SSL_free(ssl);
            error("[!] Failed failed on SSL_read");
        } else {
            printf("[+] Received %ld bytes\n", bytes_read);
        }

        printf("[+] Closing SSL Connection\n");
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    close(proxy_fd);
    SSL_CTX_free(ctx);

    /* Print Response --------------------------------------------------------------------------- */
    printf("[+] Printing Response:\n");
    print_ascii(raw_response, raw_response_l);

    return EXIT_SUCCESS;
}



static int ssl_readall(SSL *s, char **buf, size_t *buf_l, size_t *buf_sz)
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
            (*buf)[*buf_l] = '\0';
        }
    }
 
    /* receive data */
    ssize_t n = 0, ret = 0;
    while (1) {
        n = SSL_read(s, *buf + *buf_l, *buf_sz - *buf_l);
        // n = recv(s, *buf + *buf_l, *buf_sz - *buf_l, 0);
        if (n < 0) {
            error("[!] client: error occurred when SSL_reading.");
            return EXIT_FAILURE;
        } else if (n == 0) {
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

    fprintf(stderr, "[*] client: SSL_read %zu bytes\n", *buf_l);

    return EXIT_SUCCESS;
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
            (*buf)[*buf_l] = '\0';
        }
    }
 
    /* receive data */
    ssize_t n = 0, ret = 0;
    while (1) {
        n = recv(s, *buf + *buf_l, *buf_sz - *buf_l, MSG_DONTWAIT);
        if (n < 0) {
            error("[!] client: error occurred when receiving.");
            return EXIT_FAILURE;
        } else if (n == 0 || n == EAGAIN || n == EWOULDBLOCK) {
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

    fprintf(stderr, "[*] client: received %zu bytes\n", *buf_l);

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
        fprintf(stderr, "[*] client: sent %d bytes\n", n);
    }

    fprintf(stderr, "[*] client: sent %zu bytes\n", total);

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

static int save_to_file(char *uri, char *raw_response, size_t raw_response_l)
{
    if (raw_response == NULL) {
        return EXIT_FAILURE;
    }
    
    /* save raw response to file */
    char output_file[BUFSIZE + 1];
    char *basename = strrchr(uri, '/');
    if (basename == NULL) {
        basename = uri;
    } else {
        basename++;
        if (basename[0] == '\0') {
            basename = "index.html";
        }
    }

    char *body = strstr(raw_response, HEADER_END);
    if (body == NULL) {
        error("[!] Failed to find body");
        return EXIT_FAILURE;
    }
    body += HEADER_END_L;

    snprintf(output_file, BUFSIZE, "%s/%s-%s", OUTPUT_DIR, OUTPUT_FILE, basename);
    int fp = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, PERMS);
    if (fp == -1) {
        return EXIT_FAILURE;
    }

    size_t body_l = raw_response_l - (body - raw_response);
    if (write(fp, body, body_l) < 0) {
        error("[!] Failed to write to output file");
        close(fp);
        return EXIT_FAILURE;
    }
    close(fp);
    fprintf(stderr, "[+] Saved response to %s\n", output_file);

    return EXIT_SUCCESS;
}
