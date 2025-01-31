#include "proxy.h"
#include "client.h"
#include "http.h"
#include "list.h"
#include "node.h"
#include "query.h"
#include "utility.h"
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>

/* Forward declarations */
static char *get_key(Request *req);
static Node *List_add(List *list, void *data);
static int Query_connect(Query *query);

/* Buffer size */
#define BUFFER_SIZE 8192

/* Error codes */
#define SELECT_ERROR -1
#define SELECT_TIMEOUT 0
#define TIMEOUT_FALSE 0
#define TIMEOUT_TRUE 1
#define ERROR_FAILURE -1
#define ERROR_SOCKET -1
#define ERROR_SOCKET_OPTION -2
#define ERROR_TIMEOUT -3
#define ERROR_READ -4
#define ERROR_WRITE -5
#define ERROR_CONNECT -6

/* Proxy error codes */
#define PROXY_ERROR_SOCKET -100
#define PROXY_ERROR_SEND -101
#define PROXY_ERROR_RECV -102
#define PROXY_ERROR_CLOSE -103
#define PROXY_ERROR_SELECT -104
#define PROXY_ERROR_FILTERED -105
#define PROXY_ERROR_SSL -35
#define PROXY_ERROR_CONNECT -37
#define PROXY_ERROR_FETCH -39
#define PROXY_ERROR_BAD_GATEWAY -40
#define PROXY_ERROR_BAD_METHOD -41

/* Types */
#define CLIENT_TYPE 1
#define QUERY_TYPE 2

/* ----------------------------------------------------------------------------------------------
 */
/* PROXY SSL FUNCTIONS
 * -------------------------------------------------------------------------- */
#if RUN_SSL
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>

int ProxySSL_shutdown(Proxy *proxy, Client *client)
{
    if (proxy == NULL || client == NULL) {
        return ERROR_FAILURE;
    }

    int ret = SSL_shutdown(client->ssl);
    if (ret == 0) {
        ret = SSL_shutdown(client->ssl);
    }
    if (ret == -1) {
        print_error("proxy: failed to shutdown ssl");
        return PROXY_ERROR_SSL;
    }

    return EXIT_SUCCESS;
}

int ProxySSL_connect(Proxy *proxy, Query *query)
{
    if (proxy == NULL || query == NULL || query->req == NULL) {
        print_error("[proxy-ssl] invalid arguments");
        return PROXY_ERROR_SSL;
    }

    query->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (query->socket < 0) {
        print_error("[proxy-ssl] socket creation failed");
        return ERROR_SOCKET;
    }

    int flags = fcntl(query->socket, F_GETFL, 0);
    if (flags < 0) {
        print_error("[proxy-ssl] failed to get socket flags");
        return ERROR_SOCKET_OPTION;
    }

    if (fcntl(query->socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        print_error("[proxy-ssl] failed to set non-blocking mode");
        return ERROR_SOCKET_OPTION;
    }

    if (connect(query->socket, (struct sockaddr *)&query->server_addr, sizeof(query->server_addr)) < 0) {
        print_error("[proxy-ssl] connect failed");
        return ERROR_CONNECT;
    }

    // Create new SSL object for client
    query->ssl = SSL_new(proxy->ctx);
    if (query->ssl == NULL) {
        print_error("[proxy-ssl] SSL_new failed");
        ERR_print_errors_fp(stderr);
        return PROXY_ERROR_SSL;
    }

    // Set up SSL connection
    SSL_set_fd(query->ssl, query->socket);
    // Enable hostname verification
    X509_VERIFY_PARAM *param = SSL_get0_param(query->ssl);
    X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_WILDCARDS);
    if (!X509_VERIFY_PARAM_set1_host(param, query->req->host, query->req->host_l)) {
        print_error("[proxy-ssl] failed to set verification hostname");
        return PROXY_ERROR_SSL;
    }

    // Set SSL verification mode
    SSL_set_verify(query->ssl, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

    // Perform SSL handshake
    int ret = SSL_connect(query->ssl);
    if (ret <= 0) {
        int ssl_error = SSL_get_error(query->ssl, ret);
        print_error("[proxy-ssl] SSL_connect failed");
        ERR_print_errors_fp(stderr);
        
        switch (ssl_error) {
            case SSL_ERROR_ZERO_RETURN:
                print_error("[proxy-ssl] SSL connection closed cleanly");
                break;
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                print_error("[proxy-ssl] SSL operation needs to be retried");
                break;
            case SSL_ERROR_SYSCALL:
                print_error("[proxy-ssl] I/O error occurred");
                break;
            default:
                print_error("[proxy-ssl] SSL error occurred");
        }
        
        return PROXY_ERROR_SSL;
    }

    // Verify the certificate
    X509 *cert = SSL_get_peer_certificate(query->ssl);
    if (cert == NULL) {
        print_error("[proxy-ssl] server did not present a certificate");
        return PROXY_ERROR_SSL;
    }
    
    long verify_result = SSL_get_verify_result(query->ssl);
    if (verify_result != X509_V_OK) {
        print_error("[proxy-ssl] certificate verification failed");
        X509_free(cert);
        return PROXY_ERROR_SSL;
    }
    
    X509_free(cert);
    return EXIT_SUCCESS;
}

int ProxySSL_read(void *sender, int sender_type)
{
    if (sender == NULL) {
        print_error("proxyssl-read: invalid args");
        return ERROR_FAILURE;
    }

    Query *q;
    Client *c;
    ssize_t n;
    switch (sender_type) {
    case QUERY_TYPE:
        q = (Query *)sender;
        n = SSL_read(q->ssl, q->buffer + q->buffer_l, q->buffer_sz - q->buffer_l);
#if DEBUG
        print_debug("[proxyssl-read] reading from query ssl socket");
        fprintf(stderr, "[proxyssl-read] query ssl bytes read: %ld\n", n);
        print_ascii(q->buffer + q->buffer_l, n);
#endif
        if (n <= 0) {
            print_error("proxy: recv failed");
            perror("recv");
            return PROXY_ERROR_SSL;

        } else if (n < (ssize_t)(q->buffer_sz - q->buffer_l)) {
            q->buffer_l += n;
            q->res =
                Response_new(q->req->method, q->req->method_l, q->req->path, q->req->path_l, q->buffer, q->buffer_l);
            if (q->res == NULL) {
                print_error("proxy: failed to create response");
                return INVALID_RESPONSE;
            }
            q->state = QRY_RECVD_RESPONSE;
            return SERVER_RESP_RECVD;
        } else {
            q->buffer_l += n;
            if (q->buffer_l == q->buffer_sz) {
                expand_buffer(&q->buffer, &q->buffer_l, &q->buffer_sz);
            }
        }
        break;
    case CLIENT_TYPE:
        c = (Client *)sender;
        n = SSL_read(c->ssl, c->buffer + c->buffer_l, c->buffer_sz - c->buffer_l);
#if DEBUG
        print_debug("[proxyssl-read] reading from client ssl socket");
        fprintf(stderr, "[proxyssl-read] buffer_sz: %ld\n", c->buffer_sz);
        fprintf(stderr, "[proxyssl-read] buffer_l: %ld\n", c->buffer_l);
        fprintf(stderr, "[proxyssl-read] bytes to read: %ld\n", c->buffer_sz - c->buffer_l);
        fprintf(stderr, "[proxyssl-read] query ssl bytes read: %ld\n", n);
        print_ascii(c->buffer + c->buffer_l, n);
#endif
        if (n == 0) {
            print_info("proxyssl-read: client closed connection");
            return PROXY_ERROR_SSL;
        } else if (n < 0) {
            print_error("proxyssl-read: ssl read failed");
            perror("sslread");
            return PROXY_ERROR_SSL;
        } else {
            c->buffer_l += n;
            if (c->buffer_l == c->buffer_sz) {
                expand_buffer(&c->buffer, &c->buffer_l, &c->buffer_sz);
            }
        }

        /* add null terminator */
        c->buffer[c->buffer_l] = '\0';

        /* update last active time to now */
        Client_timestamp(c);
        break;
    default:
        print_error("proxy-sslread: invalid sender type");
        return ERROR_FAILURE;
    }

    return n;
}

int ProxySSL_write(Proxy *proxy, Client *client, char *buf, int len)
{
    if (proxy == NULL || client == NULL || buf == NULL || len < 0) {
        print_error("proxy_sslwrite: invalid args");
        return ERROR_FAILURE;
    }
#if DEBUG
    fprintf(stderr, "[proxyssl-write] writing to client: %d\n", client->socket);
    fprintf(stderr, "[proxyssl-write] bytes to write: %d\n", len);
    print_ascii(buf, len);
#endif
    ssize_t written  = 0;
    ssize_t to_write = len;
    int ret;
    while (to_write > 0) {
        fprintf(stderr, "[proxyssl-write] bytes written: %d\n", ret);
        ret = SSL_write(client->ssl, buf + written, to_write);
        if (ret <= 0) {
            int err = SSL_get_error(client->ssl, ret);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                continue;
            }
            print_error("proxy_sslwrite: SSL_write failed");
            return ERROR_FAILURE;
        } else {
            to_write -= ret;
        }
    }

    return written;
}

int ProxySSL_handshake(Proxy *proxy, Client *client)
{
    if (proxy == NULL) {
        print_error("proxy: accept failed, invalid args");
        return ERROR_FAILURE;
    }

    /* configure context */
    char *hostname = client->query->req->host;
    if (hostname == NULL) {
        return INVALID_REQUEST;
    }

    /* update proxy ext file ad context if needed */
    if (ProxySSL_updateExtFile(proxy, hostname) != EXIT_SUCCESS) {
        return ERROR_FAILURE;
    }

    /* create new SSL object for client */
#if DEBUG
    print_debug("[proxyssl-handshake] creating new SSL object");
#endif
    client->ssl = SSL_new(proxy->ctx);
    SSL_set_fd(client->ssl, client->socket);

    /* accept */
    if (SSL_accept(client->ssl) == -1) {
        ERR_print_errors_fp(stderr);
        print_error("proxyssl-handshake: ssl/tls handshake failed");
        /* stop SSL */
        SSL_shutdown(client->ssl);
        SSL_free(client->ssl);
        client->ssl   = NULL;
        client->isSSL = 0;
        return PROXY_ERROR_SSL;
    } else {
        print_success("proxyssl-handshake: ssl/tls connection established!");
        client->isSSL = 1;
        client->state = CLI_QUERY;
        Query_free(client->query);
        client->query = NULL;
        clear_buffer(client->buffer, &client->buffer_l);
    }

    /* update last active time to now */
    Client_timestamp(client);

    return EXIT_SUCCESS;
}

int ProxySSL_updateExtFile(Proxy *proxy, char *hostname)
{
    if (proxy == NULL || hostname == NULL) {
        print_error("proxyssl: failed to update extension file - invalid args");
        return ERROR_FAILURE;
    }

    int ret;
    fprintf(stderr, "proxyssl: updating extension file for %s\n",
            hostname); // TODO

    /* update proxy ext file */
    char command[BUFFER_SZ + 1];
    zero(command, BUFFER_SZ + 1);
    snprintf(command, BUFFER_SZ, "%s %s", UPDATE_PROXY_CERT, hostname);
    if ((ret = system(command)) < 0) {
        print_error("proxyssl: failed to update extension file - system call failed");
        return ERROR_FAILURE;
    } else if (ret > 0) {
        /* update proxy context */
        if (ProxySSL_updateContext(proxy) == ERROR_FAILURE) {
            print_error("proxyssl: failed to update extension file - update "
                        "ctx failed");
            return ERROR_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int ProxySSL_updateContext(Proxy *proxy)
{
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    /* update proxy context */
    SSL_CTX_free(proxy->ctx);
    proxy->ctx = InitServerCTX();
    if (proxy->ctx == NULL) {
        print_error("proxy: failed to update context - InitServerCTX failed");
        return ERROR_FAILURE;
    }

    if (LoadCertificates(proxy->ctx, PROXY_CERT, PROXY_KEY) == ERROR_FAILURE) {
        print_error("proxy: failed to update context - LoadCertificates failed");
        return ERROR_FAILURE;
    }

    return EXIT_SUCCESS;
}
#endif /* RUN_SSL */

static short select_loop(Proxy *proxy) {
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    int select_ret;
    while (1) {
        /* Copy master set to read set */
        proxy->readfds = proxy->master_set;

        /* Check for timeouts and update timeout value */
        if (Proxy_handleTimeout(proxy) == ERROR_FAILURE) {
            return ERROR_FAILURE;
        }

        /* Wait for activity on sockets */
        select_ret = select(proxy->fdmax + 1, &proxy->readfds, NULL, NULL, proxy->timeout);
        if (select_ret == -1) {
            if (errno == EINTR) {
                continue;
            }
            print_error("proxy: select failed");
            return PROXY_ERROR_SELECT;
        }

        /* Check for activity on listening socket */
        if (FD_ISSET(proxy->listen_fd, &proxy->readfds)) {
            if (Proxy_handleListener(proxy) < 0) {
                print_error("proxy: failed to handle listener");
                return ERROR_FAILURE;
            }
        }

        /* Handle client activity */
        if (Proxy_handle(proxy) < 0) {
            print_error("proxy: failed to handle clients");
            return ERROR_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int Proxy_run(short port) {
    struct Proxy proxy;
    signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE (broken pipe error)

    /* Initialize SSL */
#if RUN_SSL
    if (!isRoot()) {
        print_error("proxy: must be run as root");
        return ERROR_FAILURE;
    }
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();
#endif

    /* Initialize Proxy */
    Proxy_init(&proxy, port);

    /* Bind & Listen Proxy Socket */
    if (Proxy_listen(&proxy) < 0) {
        print_error("proxy: failed to listen");
        return ERROR_FAILURE;
    }

    /* Select Loop */
    short ret = select_loop(&proxy);
    if (ret == HALT) {
        print_info("proxy: shutting down");
        Proxy_free(&proxy);
        return EXIT_SUCCESS;
    }

    /* Shutdown Proxy */
    print_error("proxy: failed and cannot recover");
    Proxy_free(&proxy);

    return EXIT_FAILURE;
}

ssize_t Proxy_send(int socket, char *buffer, size_t buffer_l) {
    if (buffer == NULL) {
        print_error("proxy-send: buffer is null");
        return ERROR_FAILURE;
    }

    ssize_t written = 0, n = 0;
    size_t to_write = buffer_l;

    while ((size_t)written < buffer_l) {
        n = send(socket, buffer + written, to_write, MSG_NOSIGNAL);
        if (n <= 0) {
            if (errno == EPIPE) {
                print_error("proxy-send: broken pipe");
                return PROXY_ERROR_CLOSE;
            }
            print_error("proxy-send: send failed");
            return PROXY_ERROR_SEND;
        }
        written += n;
        to_write -= n;
    }

    return written;
}

ssize_t Proxy_recv(void *sender, int sender_type) {
    if (sender == NULL) {
        print_error("proxy_recv: invalid arguments");
        return ERROR_FAILURE;
    }

    Query *q;
    Client *c;
    int n;
    ssize_t total_bytes = 0;

    switch (sender_type) {
        case QUERY_TYPE:
            q = (Query *)sender;
            n = recv(q->socket, q->buffer + q->buffer_l, q->buffer_sz - q->buffer_l, 0);
            if (n == 0) {
                q->res = Response_new(q->req->method, q->req->method_l, q->req->path, q->req->path_l, q->buffer, q->buffer_l);
                if (q->res == NULL) {
                    return INVALID_RESPONSE;
                }
                q->state = QRY_RECVD_RESPONSE;
                return SERVER_RESP_RECVD;
            } else if (n < 0) {
                return PROXY_ERROR_RECV;
            }
            q->buffer_l += n;
            if (q->buffer_l == q->buffer_sz) {
                if (expand_buffer(&q->buffer, &q->buffer_l, &q->buffer_sz) < 0) {
                    return ERROR_FAILURE;
                }
            }
            total_bytes = (ssize_t)q->buffer_l;
            break;

        case CLIENT_TYPE:
            c = (Client *)sender;
            n = recv(c->socket, c->buffer + c->buffer_l, c->buffer_sz - c->buffer_l, 0);
            if (n == 0) {
                if (errno == ECONNRESET) {
                    c->state = CLI_CLOSE;
                    return CLIENT_CLOSE;
                }
                return EXIT_SUCCESS;
            } else if (n < 0) {
                return PROXY_ERROR_RECV;
            }
            c->buffer_l += n;
            if (c->buffer_l == c->buffer_sz) {
                if (expand_buffer(&c->buffer, &c->buffer_l, &c->buffer_sz) < 0) {
                    return ERROR_FAILURE;
                }
            }
            Client_timestamp(c);
            total_bytes = (ssize_t)c->buffer_l;
            break;

        default:
            return ERROR_FAILURE;
    }

    return total_bytes;
}

static char *get_key(Request *req) {
    if (req == NULL || req->host == NULL || req->path == NULL) {
        return NULL;
    }
    
    size_t key_len = strlen(req->host) + strlen(req->path) + 2;
    char *key = calloc(key_len, sizeof(char));
    if (key == NULL) {
        return NULL;
    }
    
    snprintf(key, key_len, "%s%s", req->host, req->path);
    return key;
}

int Proxy_serveFromCache(Proxy *proxy, Client *client, long age, char *key) {
    if (proxy == NULL || client == NULL || key == NULL || age < 0) {
        return ERROR_FAILURE;
    }

    Response *response = Cache_get(proxy->cache, key);
    if (response == NULL) {
        return ERROR_FAILURE;
    }

    /* Add Age header to response */
    char age_str[32];
    snprintf(age_str, sizeof(age_str), "%ld", age);
    
    size_t response_size = Response_size(response);
    char *response_buf = Response_get(response);
    
    if (HTTP_add_field(&response_buf, &response_size, "Age", age_str) < 0) {
        return ERROR_FAILURE;
    }

    /* Send response to client */
#if RUN_SSL
    if (client->isSSL) {
        if (ProxySSL_write(proxy, client, response_buf, response_size) <= 0) {
            free(response_buf);
            return PROXY_ERROR_SSL;
        }
    } else 
#endif
    {
        if (Proxy_send(client->socket, response_buf, response_size) < 0) {
            free(response_buf);
            return PROXY_ERROR_SEND;
        }
    }

    free(response_buf);
    return EXIT_SUCCESS;
}

ssize_t Proxy_fetch(Proxy *proxy, Query *q) {
    if (proxy == NULL || q == NULL || q->req == NULL) {
        return ERROR_FAILURE;
    }

    /* Connect to server */
    if (connect(q->socket, (struct sockaddr *)&q->server_addr, sizeof(q->server_addr)) < 0) {
        return PROXY_ERROR_BAD_GATEWAY;
    }

    /* Add socket to master set */
    FD_SET(q->socket, &proxy->master_set);
    proxy->fdmax = (q->socket > proxy->fdmax) ? q->socket : proxy->fdmax;

    /* Send request to server */
    ssize_t bytes_sent = Proxy_send(q->socket, q->req->raw, q->req->raw_l);
    if (bytes_sent < 0) {
        return PROXY_ERROR_FETCH;
    }

    return bytes_sent;
}

int Proxy_handleQuery(Proxy *proxy, Query *query, int isSSL) {
    if (proxy == NULL || query == NULL) {
        return ERROR_FAILURE;
    }

    ssize_t bytes_received;
    if (isSSL) {
#if RUN_SSL
        bytes_received = ProxySSL_read(query, QUERY_TYPE);
#else
        return PROXY_ERROR_SSL;
#endif
    } else {
        bytes_received = Proxy_recv(query, QUERY_TYPE);
    }

    if (bytes_received < 0) {
        return PROXY_ERROR_RECV;
    }

    if (bytes_received == SERVER_RESP_RECVD) {
        query->state = QRY_RECVD_RESPONSE;
    }

    return EXIT_SUCCESS;
}

int Proxy_sendServerResp(Proxy *proxy, Client *client) {
    if (proxy == NULL || client == NULL || client->query == NULL || client->query->res == NULL) {
        return ERROR_FAILURE;
    }

    /* Get response from query */
    char *response_buf = calloc(client->query->res->raw_l + 1, sizeof(char));
    if (response_buf == NULL) {
        return ERROR_FAILURE;
    }
    memcpy(response_buf, client->query->res->raw, client->query->res->raw_l);

    /* Color links if enabled */
#if RUN_COLOR
    char **key_array = Cache_getKeyList(proxy->cache);
    int num_keys = (int)proxy->cache->size;
    if (color_links(&response_buf, &client->query->res->raw_l, key_array, num_keys) != 0) {
        free(response_buf);
        return ERROR_FAILURE;
    }
#endif

    /* Send response to client */
#if RUN_SSL
    if (client->isSSL) {
        if (ProxySSL_write(proxy, client, response_buf, client->query->res->raw_l) <= 0) {
            free(response_buf);
            return PROXY_ERROR_SSL;
        }
    } else
#endif
    {
        if (Proxy_send(client->socket, response_buf, client->query->res->raw_l) < 0) {
            free(response_buf);
            return PROXY_ERROR_SEND;
        }
    }

    /* Cache the response */
#if RUN_CACHE
    char *key = get_key(client->query->req);
    if (key != NULL) {
        Response *cached_res = Response_copy(client->query->res);
        if (cached_res != NULL) {
            Cache_put(proxy->cache, key, cached_res, cached_res->max_age);
        }
        free(key);
    }
#endif

    free(response_buf);
    return EXIT_SUCCESS;
}

int Proxy_init(Proxy *proxy, short port) {
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    /* Initialize cache if enabled */
#if RUN_CACHE
    proxy->cache = Cache_new(CACHE_SZ, Response_free, Response_print);
    if (proxy->cache == NULL) {
        return ERROR_FAILURE;
    }
#endif

    /* Initialize filter list if enabled */
#if RUN_FILTER
    Proxy_readFilterList(proxy);
#endif

    /* Initialize socket structures */
    zero(&(proxy->addr), sizeof(proxy->addr));
    proxy->listen_fd = -1;
    proxy->port = port;

    /* Initialize fd sets */
    FD_ZERO(&(proxy->master_set));
    FD_ZERO(&(proxy->readfds));
    proxy->fdmax = 0;
    proxy->timeout = NULL;

    /* Initialize client list */
    proxy->client_list = List_new(Client_free, Client_print, Client_compare);
    if (proxy->client_list == NULL) {
        return ERROR_FAILURE;
    }

    /* Initialize SSL context if enabled */
#if RUN_SSL
    proxy->ctx = InitServerCTX();
    if (proxy->ctx == NULL) {
        return ERROR_FAILURE;
    }
    LoadCertificates(proxy->ctx, PROXY_CERT, PROXY_KEY);
#endif

    return EXIT_SUCCESS;
}

void Proxy_free(void *proxy) {
    if (proxy == NULL) {
        return;
    }

    Proxy *p = (Proxy *)proxy;

#if RUN_SSL
    if (p->ctx != NULL) {
        SSL_CTX_free(p->ctx);
    }
#endif

#if RUN_CACHE
    Cache_free(&p->cache);
#endif

#if RUN_FILTER
    Proxy_freeFilters(p);
#endif

    List_free(&p->client_list);
    
    if (p->listen_fd != -1) {
        close(p->listen_fd);
        p->listen_fd = -1;
    }

    if (p->timeout != NULL) {
        free(p->timeout);
        p->timeout = NULL;
    }
}

int Proxy_listen(Proxy *proxy) {
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    /* Create listening socket */
    proxy->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy->listen_fd == -1) {
        return ERROR_FAILURE;
    }

    /* Set socket options */
    int optval = 1;
    if (setsockopt(proxy->listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        close(proxy->listen_fd);
        return ERROR_FAILURE;
    }

    /* Set up server address structure */
    proxy->addr.sin_family = AF_INET;
    proxy->addr.sin_addr.s_addr = htonl(INADDR_ANY);
    proxy->addr.sin_port = htons(proxy->port);

    /* Bind socket */
    if (bind(proxy->listen_fd, (struct sockaddr *)&proxy->addr, sizeof(proxy->addr)) == -1) {
        close(proxy->listen_fd);
        return ERROR_FAILURE;
    }

    /* Listen for connections */
    if (listen(proxy->listen_fd, SOMAXCONN) == -1) {
        close(proxy->listen_fd);
        return ERROR_FAILURE;
    }

    /* Add listening socket to master set */
    FD_SET(proxy->listen_fd, &proxy->master_set);
    proxy->fdmax = proxy->listen_fd;

    return EXIT_SUCCESS;
}

int Proxy_handleListener(Proxy *proxy) {
    return Proxy_accept(proxy);
}

int Proxy_accept(Proxy *proxy) {
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd;

    /* Accept new connection */
    client_fd = accept(proxy->listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd == -1) {
        return ERROR_FAILURE;
    }

    /* Create new client */
    Client *client = Client_create(client_fd);
    if (client == NULL) {
        close(client_fd);
        return ERROR_FAILURE;
    }

    /* Set client address */
    if (Client_setAddr(client, &client_addr) < 0) {
        Client_free(client);
        return ERROR_FAILURE;
    }

    /* Add client to list */
    Node *node = List_add(proxy->client_list, client);
    if (node == NULL) {
        Client_free(client);
        return ERROR_FAILURE;
    }

    /* Add client socket to master set */
    FD_SET(client_fd, &proxy->master_set);
    if (client_fd > proxy->fdmax) {
        proxy->fdmax = client_fd;
    }

    return EXIT_SUCCESS;
}

void Proxy_close(int socket, fd_set *master_set, List *client_list, Client *client) {
    if (socket < 0 || master_set == NULL || client_list == NULL || client == NULL) {
        return;
    }

    /* Remove socket from master set */
    FD_CLR(socket, master_set);

    /* Close socket */
    close(socket);

    /* Remove client from list */
    List_remove(client_list, client);
}

#if RUN_FILTER
void Proxy_freeFilters(Proxy *proxy) {
    if (proxy == NULL) {
        return;
    }

    for (int i = 0; i < proxy->num_filters; i++) {
        if (proxy->filters[i] != NULL) {
            free(proxy->filters[i]);
            proxy->filters[i] = NULL;
        }
    }
    proxy->num_filters = 0;
}

int Proxy_readFilterList(Proxy *proxy) {
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    FILE *fp = fopen("filters.txt", "r");  // Use a constant or config value for filter file
    if (fp == NULL) {
        return ERROR_FAILURE;
    }

    char line[1024];  // Use a reasonable buffer size
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* Remove newline */
        line[strcspn(line, "\n")] = 0;
        
        if (Proxy_addFilter(proxy, line) < 0) {
            fclose(fp);
            return ERROR_FAILURE;
        }
    }

    fclose(fp);
    return EXIT_SUCCESS;
}

int Proxy_addFilter(Proxy *proxy, char *filter) {
    if (!proxy || !filter || proxy->num_filters >= MAX_FILTERS) {
        return -1;
    }
    
    // Create a copy of the filter string
    char *filter_copy = strdup(filter);
    if (!filter_copy) {
        return -1;
    }
    
    // Add the filter to the array
    proxy->filters[proxy->num_filters++] = filter_copy;
    return 0;
}

bool Proxy_isFiltered(Proxy *proxy, char *host) {
    if (!proxy || !host) {
        return false;
    }
    
    // Check each filter
    for (int i = 0; i < proxy->num_filters; i++) {
        if (proxy->filters[i] && strstr(host, proxy->filters[i]) != NULL) {
            return true;
        }
    }
    
    return false;
}

int Proxy_sendError(Client *client, int msg_code) {
    char *error_msg;
    switch (msg_code) {
        case 400:
            error_msg = "HTTP/1.0 400 Bad Request\r\n\r\n";
            break;
        case 403:
            error_msg = "HTTP/1.0 403 Forbidden\r\n\r\n";
            break;
        case 404:
            error_msg = "HTTP/1.0 404 Not Found\r\n\r\n";
            break;
        case 500:
            error_msg = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
            break;
        case 501:
            error_msg = "HTTP/1.0 501 Not Implemented\r\n\r\n";
            break;
        case 502:
            error_msg = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
            break;
        case 503:
            error_msg = "HTTP/1.0 503 Service Unavailable\r\n\r\n";
            break;
        default:
            error_msg = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
    }
    return write(client->socket, error_msg, strlen(error_msg));
}
#endif

int Proxy_handleCONNECT(Proxy *proxy, Client *client) {
    if (proxy == NULL || client == NULL || client->query == NULL) {
        return ERROR_FAILURE;
    }

    int ret;
    Query *query = client->query;

    /* Check if host is filtered */
#if RUN_FILTER
    if (Proxy_isFiltered(proxy, query->req->host)) {
        return PROXY_ERROR_FILTERED;
    }
#endif

    /* Connect to server if not already connected */
    if (query->state == 0) {  // Initial state
        ret = Query_connect(query);
        if (ret < 0) {
            return ret;
        }
        query->state = 1;  // Connected state
    }

    /* Send 200 OK to client */
    if (query->state == 1) {  // Connected state
        ret = Proxy_send(client->socket, "HTTP/1.1 200 Connection established\r\n\r\n", 39);
        if (ret < 0) {
            return ret;
        }
        query->state = 2;  // Tunnel state
        client->state = CLI_TUNNEL;
    }

    return EXIT_SUCCESS;
}

int Proxy_handleClient(Proxy *proxy, Client *client) {
    if (proxy == NULL || client == NULL) {
        return ERROR_FAILURE;
    }

    int ret;
    ssize_t n;

    /* Receive data from client */
    n = Proxy_recv(client, CLIENT_TYPE);
    if (n < 0) {
        return n;
    }

    /* Parse request if no request exists */
    if (!client->hasRequest) {
        Request *req;
        if (Query_new(&client->query, client->buffer, client->buffer_l) < 0) {
            return ERROR_FAILURE;
        }
        client->hasRequest = true;
    }

    /* Handle request based on method */
    if (strcmp(client->query->req->method, "CONNECT") == 0) {
        client->state = CLI_CONNECT;
        ret = Proxy_handleCONNECT(proxy, client);
    } else if (strcmp(client->query->req->method, "GET") == 0) {
        client->state = CLI_GET;
        ret = Proxy_handleGET(proxy, client);
    } else {
        ret = ERROR_FAILURE;  // Invalid method
    }

    return ret;
}

int Proxy_handleEvent(Proxy *proxy, Client *client, int error_code) {
    if (proxy == NULL || client == NULL) {
        return ERROR_FAILURE;
    }

    switch (error_code) {
        case CLIENT_CLOSE:
        case PROXY_ERROR_CLOSE:
        case PROXY_ERROR_SEND:
        case PROXY_ERROR_RECV:
            Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client);
            break;
        case ERROR_FAILURE:
            Proxy_sendError(client, error_code);
            Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client);
            break;
        default:
            return error_code;
    }

    return EXIT_SUCCESS;
}

int Proxy_handleTunnel(int sender, int receiver) {
    char buffer[BUFFER_SIZE];
    ssize_t n;

    /* Read from sender */
    n = recv(sender, buffer, sizeof(buffer), 0);
    if (n <= 0) {
        return (n == 0) ? CLIENT_CLOSE : PROXY_ERROR_RECV;
    }

    /* Send to receiver */
    ssize_t written = 0;
    while (written < n) {
        ssize_t bytes = send(receiver, buffer + written, n - written, MSG_NOSIGNAL);
        if (bytes <= 0) {
            return PROXY_ERROR_SEND;
        }
        written += bytes;
    }

    return EXIT_SUCCESS;
}

int Proxy_handle(Proxy *proxy) {
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    int ret = EXIT_SUCCESS;
    Client *client = NULL;
    Node *curr = NULL, *next = NULL;

    for (curr = proxy->client_list->head; curr != NULL; curr = next) {
        next = curr->next;
        client = (Client *)curr->data;

        /* Handle client in query or init state */
        if ((client->state == CLI_QUERY || client->state == CLI_INIT) && 
            FD_ISSET(client->socket, &proxy->readfds)) {
            ret = Proxy_handleClient(proxy, client);
            if (ret < 0) {
                goto event_handler;
            }
        }

        /* Handle client based on state */
        switch (client->state) {
            case CLI_GET:
                ret = Proxy_handleGET(proxy, client);
                break;
            case CLI_CONNECT:
                ret = Proxy_handleCONNECT(proxy, client);
                break;
#if RUN_SSL
            case CLI_SSL:
                if (FD_ISSET(client->socket, &proxy->readfds)) {
                    ret = ProxySSL_handshake(proxy, client);
                }
                break;
#endif
            case CLI_TUNNEL:
                if (FD_ISSET(client->socket, &proxy->readfds)) {
                    ret = Proxy_handleTunnel(client->socket, client->query->socket);
                }
                if (FD_ISSET(client->query->socket, &proxy->readfds)) {
                    ret = Proxy_handleTunnel(client->query->socket, client->socket);
                }
                break;
        }

event_handler:
        if (ret != EXIT_SUCCESS) {
            ret = Proxy_handleEvent(proxy, client, ret);
            if (ret != EXIT_SUCCESS) {
                return ret;
            }
        }
    }

#if RUN_CACHE
    Cache_refresh(proxy->cache);
#endif

    return EXIT_SUCCESS;
}

int Proxy_handleTimeout(Proxy *proxy) {
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    double now = get_current_time();
    double timeout = TIMEOUT_THRESHOLD;
    Node *curr = proxy->client_list->head;

    while (curr != NULL) {
        Client *client = (Client *)curr->data;
        Node *next = curr->next;

        double age = now - timeval_to_double(client->last_active);
        if (age > TIMEOUT_THRESHOLD) {
            Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client);
        } else {
            double time_til_timeout = TIMEOUT_THRESHOLD - age;
            if (time_til_timeout < timeout) {
                timeout = time_til_timeout;
            }
        }
        curr = next;
    }

    if (timeout == TIMEOUT_THRESHOLD) {
        if (proxy->timeout != NULL) {
            free(proxy->timeout);
            proxy->timeout = NULL;
        }
        return TIMEOUT_FALSE;
    }

    if (proxy->timeout == NULL) {
        proxy->timeout = calloc(1, sizeof(struct timeval));
        if (proxy->timeout == NULL) {
            return ERROR_FAILURE;
        }
    }

    double_to_timeval(proxy->timeout, timeout);
    return TIMEOUT_TRUE;
}

int Proxy_handleGET(Proxy *proxy, Client *client)
{
    int ret = EXIT_SUCCESS;
    if (client->query->state == QRY_INIT) { // check cache, if sent request, already checked cache
#if RUN_CACHE
        char *key = get_key(client->query->req);
        if (key != NULL) {
            Response *cache_res = Cache_get(proxy->cache, key);
            if (cache_res != NULL) {
                long cache_res_age = Cache_get_age(proxy->cache, key);
                ret = Proxy_serveFromCache(proxy, client, cache_res_age, key);
                if (ret < 0) {
                    print_error("proxy: failed to serve from cache");
                    free(key);
                    return ret;
                }
                free(key);
                return EXIT_SUCCESS;
            }
            free(key);
        }
#endif
        /* make connection to server */
        client->query->socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client->query->socket < 0) {
            print_error("proxy: failed to create socket");
            return ERROR_SOCKET;
        }

        /* connect to server */
        ret = (client->isSSL) ? ProxySSL_connect(proxy, client->query) : Proxy_fetch(proxy, client->query);

#if DEBUG
        fprintf(stderr, "[proxy-handle-get] connect returned %d\n", ret);
#endif
        if (ret < 0) {
            print_error("proxy: failed to connect to server");
            if (client->query->socket != -1) {
                close(client->query->socket);
                client->query->socket = -1;
            }
            return ret;
        }
        client->query->state = QRY_SENT_REQUEST;
    } else if (client->query->state == QRY_SENT_REQUEST) {
        if (FD_ISSET(client->query->socket, &proxy->readfds)) {
            ret = Proxy_handleQuery(proxy, client->query, client->isSSL);
#if DEBUG
            fprintf(stderr, "[proxy-handle-get] handle query returned %d\n", ret);
#endif
            if (ret < 0) {
                print_error("proxy: failed to handle query");
                return ret;
            }
        }
    } else if (client->query->state == QRY_RECVD_RESPONSE) {
#if DEBUG
        print_info("[proxy-handle-get] sending server response to client");
#endif
        ret = Proxy_sendServerResp(proxy, client);
        if (ret < 0) {
            print_error("proxy: failed to send server response to client");
            return ret;
        }
#if DEBUG
        print_success("[proxy-handle-get] sent server response to client");
#endif
    }

    return ret;
}

Node *List_add(List *list, void *data) {
    if (list == NULL || data == NULL) {
        return NULL;
    }

    Node *node = (Node *)malloc(sizeof(Node));
    if (node == NULL) {
        return NULL;
    }

    node->data = data;
    node->next = NULL;

    if (list->head == NULL) {
        list->head = node;
    } else {
        Node *curr = list->head;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = node;
    }

    return node;
}

int Query_connect(Query *query) {
    if (query == NULL) {
        return ERROR_FAILURE;
    }

    /* Connect to server */
    if (connect(query->socket, (struct sockaddr *)&query->server_addr, sizeof(query->server_addr)) < 0) {
        return PROXY_ERROR_BAD_GATEWAY;
    }

    return EXIT_SUCCESS;
}
