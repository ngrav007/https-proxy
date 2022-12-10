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
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFSIZE 1024
#define OUTPUT_DIR "output"
#define OUTPUT_FILE "resp"
#define SAVE_FILE 1
#define PERMS 0644

static int sendall(int s, char *buf, size_t len);
static int recvall(int s, char **buf, size_t *buf_l, size_t *buf_sz);
static void error(char *msg);
static int connect_to_proxy(char *host, int port);
static int save_to_file(char *uri, char *raw_response, size_t raw_response_l);
static int sslreadall(SSL *s, char **buf, size_t *buf_l, size_t *buf_sz);

int main(int argc, char **argv)
{
    /* check command line arguments */
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <proxy-host> <proxy-port> <method> <host> [uri]\n", argv[0]);
        exit(EXIT_FAILURE);
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
    char port[MAX_DIGITS_LONG];
    zero(port, MAX_DIGITS_LONG);
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
    
    /* SSL Initialize */
    SSL *ssl;
    SSL_CTX *ctx;
    SSL_library_init();
    ctx = init_ctx();

    /* connect to proxy */
    int proxy_fd = connect_to_proxy(proxy_host, proxy_port);
    if (proxy_fd < 0) {
        error("[!] Failed to connect to proxy");
    }

    /* TLS/SSL Initialize */
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, proxy_fd);
    // if (SSL_connect(ssl) == -1) {
    //     ERR_print_errors_fp(stderr);
    // } else {
    //     printf("[+] Connected with %s encryption\n", SSL_get_cipher(ssl));
    // }

    /* build raw request */
    size_t raw_l = 0;
    char *raw_request = Raw_request(method, uri, host, port, NULL, &raw_l);
    if (raw_request == NULL) {
        close(proxy_fd);
        error("[!] Failed to build raw request");
    }

    /* add non-persistent connection field */
    // if (HTTP_add_field(&raw_request, &raw_l, "Connection", "close") < 0) {
    //     close(proxy_fd);
    //     error("[!] Failed to add connection field");
    // }

    fprintf(stderr, "[+] Sending request:\n%s", raw_request);
    fprintf(stderr, "[+] Length: %zu\n", raw_l);


    /* send raw request */
    if (sendall(proxy_fd, raw_request, raw_l) < 0) {
        free(raw_request);
        close(proxy_fd);
        error("[!] Failed to send raw request");
    }

    if (SSL_connect(ssl) == -1) {
        ERR_print_errors_fp(stderr);
    }

    /* receive raw response */
    size_t raw_response_l = 0;
    size_t raw_response_sz = 0;
    char *raw_response = NULL;
    if (recvall(proxy_fd, &raw_response, &raw_response_l, &raw_response_sz) < 0) {
        free(raw_request);
        close(proxy_fd);
        error("[!] Failed to receive raw response");
    }

    fprintf(stderr, "[*] HTTP Response Header --------------------- +\n\n");
    char *header_end = strstr(raw_response, "\r\n\r\n");
    if (header_end == NULL) {
        free(raw_request);
        free(raw_response);
        close(proxy_fd);
        error("[!] Failed to find header end");
    }   
    print_ascii(raw_response, header_end - raw_response);
    fprintf(stderr, "\n[*] --------------------------------------------\n");

    if (strstr(raw_response, "200 OK") != NULL) {  // Connection Established
        // if (SSL_connect(ssl) == -1) {
        //     ERR_print_errors_fp(stderr);
        // } 
        // else {
            printf("[+] Connected with %s encryption\n", SSL_get_cipher(ssl));

            display_certs(ssl);

            // Send a GET request
            char *method = "GET";
            char *uri = "/comp/112/index.html";
            char *host = "www.cs.tufts.edu";
            char *port = "443";

            char *get_request = Raw_request(method, uri, host, port, NULL, &raw_l);
            if (get_request == NULL) {
                SSL_free(ssl);
                SSL_CTX_free(ctx);
                free(raw_request);
                free(raw_response);
                close(proxy_fd);
                error("[!] Failed to build raw request");
            }
            
            // Create a follow-up get request to send to same host through proxy 
            
            SSL_write(ssl, get_request, sizeof(get_request));
            
            
            // Read the response from dest server that proxy re-encrypted 
            char *ssl_response = NULL;
            size_t ssl_response_l = 0;
            size_t ssl_response_sz = 0;
            if (sslreadall(ssl, &ssl_response, &ssl_response_l, &ssl_response_sz) < 0) {
                SSL_free(ssl);
                SSL_CTX_free(ctx);
                free(raw_request);
                free(raw_response);
                close(proxy_fd);
                error("[!] Failed to read ssl response.");

            }

            fprintf(stderr, "[*] HTTPS (TLS) Response Header ---------- +\n\n");
            char *header_response_end = strstr(ssl_response, "\r\n\r\n");
            if (header_response_end == NULL) {
                SSL_free(ssl);
                SSL_CTX_free(ctx);
                free(raw_request);
                free(raw_response);
                close(proxy_fd);
                error("[!] Failed to find response header end");
            }   
            print_ascii(ssl_response, header_response_end - ssl_response);
            fprintf(stderr, "\n[*] --------------------------------------------\n");
        // }
    }

    // free(raw_request); // if save to file fails free this stuff
    // free(raw_response);
    // close(proxy_fd);
    // error("[!] Failed to open output file");

    SSL_free(ssl);
    SSL_CTX_free(ctx);
    free(raw_request);
    free(raw_response);
    close(proxy_fd);

    return EXIT_SUCCESS;
}

static int sslreadall(SSL *s, char **buf, size_t *buf_l, size_t *buf_sz)
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
        n = recv(s, *buf + *buf_l, *buf_sz - *buf_l, 0);
        if (n < 0) {
            error("[!] client: error occurred when receiving.");
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


// SSL_CTX* init_ctx(void)
// {
//     SSL_METHOD *method;
//     SSL_CTX *ctx;
//     OpenSSL_add_all_algorithms();  /* Load cryptos, et.al. */
//     SSL_load_error_strings();   /* Bring in and register error messages */
//     method = TLS_client_method();  /* Create new client-method instance */
//     ctx = SSL_CTX_new(method);   /* Create new context */
//     if ( ctx == NULL )
//     {
//         fprintf(stderr, "[!] Failed to initiate context\n");
//         return EXIT_FAILURE;
//     }
//     return ctx;
// }