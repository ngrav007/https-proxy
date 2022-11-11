#include "proxy.h"

// Ports -- 9055 to 9059

#define DEBUG 0

static void zero(void *p, size_t n);
static int expand_buffer(struct Proxy *proxy);
static int compact_buffer(struct Proxy *proxy);
static void clear_buffer(struct Proxy *proxy);

int Proxy_run(short port, size_t cache_size)
{
    struct Proxy proxy;

    /* initialize the proxy */
    Proxy_init(&proxy, port, cache_size);

    /* open listening socket */
    proxy.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy.listen_fd == -1) {
        fprintf(stderr, "[Error] Proxy_run: socket failed\n");
        Proxy_free(&proxy);
        return -1;
    }

    /* set socket options */
    int optval = 1;
    if (setsockopt(proxy.listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) == -1)
    {
        fprintf(stderr, "[Error] Proxy_run: setsockopt failed\n");
        Proxy_free(&proxy);
        return -1;
    }

    /* build proxy address (type is: struct sockaddr_in) */
    zero(&proxy.addr, sizeof(proxy.addr));
    proxy.addr.sin_family      = AF_INET;
    proxy.addr.sin_addr.s_addr = htonl(INADDR_ANY);
    proxy.addr.sin_port        = htons(proxy.port);

    /* bind listening socket to proxy address; binding listent_fd==mastersocket */
    if (bind(proxy.listen_fd, (struct sockaddr *)&proxy.addr,
             sizeof(proxy.addr)) == -1)
    {
        fprintf(stderr, "[Error] Proxy_run: bind failed - %s\n",
                strerror(errno));
        Proxy_free(&proxy);
        return -1;
    }

    /* listen for connections */
    if (listen(proxy.listen_fd, 10) == -1) {
        fprintf(stderr, "[Error] Proxy_run: listen failed\n");
        Proxy_free(&proxy);
        return -1;
    }

    /******************* Main accept loop *******************/
    short flag           = 0;
    socklen_t client_len = sizeof(proxy.client_addr);
    while (true) {
        /* accept a connection */
        proxy.client_fd =
            accept(proxy.listen_fd, (struct sockaddr *)&proxy.client_addr,
                   &client_len);
        if (proxy.client_fd == -1) {
            fprintf(stderr, "[Error] Proxy_run: accept failed\n");
            Proxy_free(&proxy);
            return -1;
        }

#if DEBUG
        fprintf(stderr, "%s[+] Proxy accepted connection from %s:%d%s\n", GRN,
                inet_ntoa(proxy.client_addr.sin_addr),
                ntohs(proxy.client_addr.sin_port), reset);
#endif

        /* handle the connection */
        if ((flag = Proxy_handle(&proxy)) == 1) {
#if DEBUG
            fprintf(stderr,
                    "%s[!] Shutdown signal received. Halting Proxy.%s\n", MAG,
                    reset);
#endif
            break;
        } else if (flag == -1) {
            close(proxy.client_fd);
            proxy.client_fd = -1;
            continue;
        }

#if DEBUG
        fprintf(stderr, "%s[+] Proxy closing connection from %s:%d%s\n", BLU,
                inet_ntoa(proxy.client_addr.sin_addr),
                ntohs(proxy.client_addr.sin_port), reset);
#endif

        /* close the connection */
        close(proxy.client_fd);
        proxy.client_fd = -1;
    } 
    /******************* End of main accept loop *******************/


    /******************* New accept loop *******************/
    /* add mastersocket to master set; make current maskersocket the fdmax */
    FD_SET(proxy.listen_fd, &(proxy.master_set)); 
    proxy.fdmax = proxy.listen_fd;
    
    while (true) {
        proxy.readfds = proxy.master_set;

        /* blocks until at least 1 socket is ready to either: 
           1) masterSocket at the start of the program when no clients 
              are connected, or
           2) client sockets that are trying to send data. 
           3) someone timed-out (proxy's timer is initialized to null, but in reading data, clients' timers starts, and those timers with less time to a timeout gets their timeval copied to proxy's timeval). For case #3, iterate through clients as if you are searching for someone to read from, but boot if you find someone has timedout. Don't need to check if select() returned a 0 for timeout(?) */
        if (select(proxy.fdmax + 1, &(proxy.readfds), NULL, NULL, proxy.timeout) < 0) { // ?? < 0 or  == -1
            fprintf(stderr, "[Error] Proxy_run: select failed\n");
            // ?? do we quit if a select fails?
            Proxy_free(&proxy);
            return -1;
        }

        /* handle new client request */
        if (FD_ISSET(proxy.listen_fd, &(proxy.readfds))) {
            /* Accept new client and add to master set */
            // Note: Make a new persisting "client"-struct since we are recv()'ing data one read at a time 

            if (Proxy_accept_new_client(&proxy) < 0) {
                /* handle when accept fails */
            }

        } 
        /* otherwise: handle either recv()'ing from a client or dealing with a timeout */
        else {

            // iterate thorugh client list, and recv' from or check timeout?

        }



    }
    /******************* End of new accept loop *******************/

    Proxy_free(&proxy);

    return EXIT_SUCCESS;
}

int Proxy_handle(struct Proxy *proxy)
{
    if (proxy == NULL) {
        fprintf(stderr, "[Error] Proxy_handle: NULL parameter not allowed\n");
        return -1;
    }

    /* get connection info */
    proxy->client =
        gethostbyaddr((const char *)&proxy->client_addr.sin_addr.s_addr,
                      sizeof(proxy->client_addr.sin_addr.s_addr), AF_INET);
    if (proxy->client == NULL) {
        fprintf(stderr, "[Error] Proxy_handle: gethostbyaddr failed\n");
        return -1;
    }

    proxy->client_ip = inet_ntoa(proxy->client_addr.sin_addr);
    if (proxy->client_ip == NULL) {
        fprintf(stderr, "[Error] Proxy_handle: inet_ntoa failed\n");
        return -1;
    }

    /* read request */
    errno = 0;
    if (Proxy_read(proxy, proxy->client_fd) == -1) {
        fprintf(stderr, "[Error] Proxy_handle: Proxy_read failed: %s\n",
                strerror(errno));
        return -1;
    }

    /* parse request */
    HTTP_Request request = HTTP_parse_request(proxy->buffer, proxy->buffer_l);
    if (request == NULL) {
        fprintf(stderr, "[Error] Proxy_handle: parse_request failed\n");
        return -1;
    }

    /* check for halt signal */
    if (strstr(request->request, "___HALT___") != NULL) {
        HTTP_free_request(request);
        return 1;
    }

    /* construct key */
    char key[request->host_l + request->path_l + 1];
    zero(key, sizeof(key));
    memcpy(key, request->host, request->host_l);
    memcpy(key + request->host_l, request->path, request->path_l);

    /* refresh cache and check if key exists */
    Cache_refresh(proxy->cache);
    struct Connection *conn = NULL;
    struct Entry *e         = NULL;
    if (proxy->cache->size > 0) {
        e = Cache_find(proxy->cache, key);
        /* if entry is stale, delete it */
        if (e != NULL && e->stale) {
            List_remove(proxy->cache->lru, e);
            Entry_delete(e, proxy->cache->free_foo);
            e = NULL;
        } else if (e != NULL && (!e->stale || e->deleted)) {
            conn                = e->value;
            conn->is_from_cache = true;
            HTTP_free_request(request);
            goto send_response;
        }
    }

    /* If not found, then fetch the response from the server */
    short server_port = HTTP_get_port(request);
    if (server_port == -1) {
        fprintf(stderr, "[Error] Proxy_handle: get_port failed\n");
        return -1;
    }

    /* open a socket to the server */
    proxy->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy->server_fd == -1) {
        fprintf(stderr, "[Error] Proxy_handle: socket failed\n");
        return -1;
    }

    /* get server information */
    errno         = 0;
    proxy->server = gethostbyname(request->host);
    if (proxy->server == NULL) {
        fprintf(stderr, "[Error] gethostbyname failed : %s\n",
                hstrerror(h_errno));
        HTTP_free_request(request);
        return -1;
    }

    /* build server address */
    zero(&proxy->server_addr, sizeof(proxy->server_addr));
    proxy->server_addr.sin_family = AF_INET;
    bcopy((char *)proxy->server->h_addr,
          (char *)&proxy->server_addr.sin_addr.s_addr, proxy->server->h_length);
    proxy->server_addr.sin_port = htons(server_port);

    /* connect to server */
    if (connect(proxy->server_fd, (struct sockaddr *)&proxy->server_addr,
                sizeof(proxy->server_addr)) == -1)
    {
        fprintf(stderr, "[Error] Proxy_handle: connect failed\n");
        close(proxy->server_fd);
        return -1;
    }

    /* send request to server */
    if (Proxy_write(proxy, proxy->server_fd) == -1) {
        fprintf(stderr, "[Error] Proxy_handle: Proxy_write failed\n");
        close(proxy->server_fd);
        return -1;
    }

    /* read response from server */
    errno = 0;
    if ((Proxy_read(proxy, proxy->server_fd)) == -1) {
        fprintf(stderr, "[Error] Proxy_handle: Proxy_read failed: %s\n",
                strerror(errno));
        close(proxy->server_fd);
        return -1;
    }

    /* parse response */
    HTTP_Response response =
        HTTP_parse_response(proxy->buffer, proxy->buffer_l);
    if (response == NULL) {
        fprintf(stderr, "[Error] Proxy_handle: parse_response failed\n");
        close(proxy->server_fd);
        return -1;
    }

    /* create a new connection */
    conn = Connection_new(request, response);
    if (conn == NULL) {
        fprintf(stderr, "[Error] Proxy_handle: Connection_new failed\n");
        close(proxy->server_fd);
        return -1;
    }

    /* add connection to cache */
    if (Cache_put(proxy->cache, key, conn, response->max_age) == -1) {
        fprintf(stderr, "[Error] Proxy_handle: Cache_put failed\n");
        close(proxy->server_fd);
        return -1;
    }

    /* close the connection to the server */
    close(proxy->server_fd);

    /* Get the connection from the cache */
    conn = Cache_get(proxy->cache, key);
    if (conn == NULL) {
        fprintf(stderr, "[Error] Proxy_handle: Cache_get failed\n");
        return -1;
    }
    

send_response: ;
    /* get the current age of the connection */
    long age = Cache_get_age(proxy->cache, key);

    /* populate the buffer with the response */
    clear_buffer(proxy);
    size_t response_len = 0;
    char *response_str  = HTTP_response_to_string(
         conn->response, age, &response_len, conn->is_from_cache);
    if (response_str == NULL) {
        fprintf(stderr,
                "[Error] Proxy_handle: HTTP_response_to_string failed\n");
        return -1;
    }

    memcpy(proxy->buffer, response_str, response_len);
    proxy->buffer_l = response_len;
    free(response_str);

    /* send the response to the client */
    if (Proxy_write(proxy, proxy->client_fd) == -1) {
        return -1;
    }

    return 0;
}

ssize_t Proxy_write(struct Proxy *proxy, int socket)
{
    if (proxy == NULL) {
        fprintf(stderr, "[Error] Proxy_write: NULL parameter not allowed\n");
        return -1;
    }

    /* write request to server */
    size_t bytes_written  = 0;
    size_t bytes_to_write = proxy->buffer_l;
    while (bytes_written < proxy->buffer_l) {
        ssize_t n =
            write(socket, proxy->buffer + bytes_written, bytes_to_write);
        if (n == -1) {
            fprintf(stderr, "%s[Error]%s write failed: %s\n", RED, reset,
                    strerror(errno));
            return -1;
        }
        bytes_written += (size_t)n;
        bytes_to_write -= (size_t)n;
    }

    return bytes_written;
}

ssize_t Proxy_read(struct Proxy *proxy, int socket)
{
    if (proxy == NULL) {
        fprintf(stderr, "[Error] Proxy_read: NULL parameter not allowed\n");
        return -1;
    }

    ssize_t bytes_read     = 0;
    size_t bytes_to_read   = proxy->buffer_sz - proxy->buffer_l - 1;
    size_t body_bytes_read = 0;
    clear_buffer(proxy);
    while ((bytes_read = read(socket, proxy->buffer + proxy->buffer_l,
                              bytes_to_read)) > 0)
    {
        /* update buffer length */
        proxy->buffer_l += bytes_read;

        /* check if buffer is full */
        if (proxy->buffer_l == (proxy->buffer_sz - 1)) {
            if (expand_buffer(proxy) == -1) {
                fprintf(stderr, "[Error] Proxy_read: expand_buffer failed\n");
                return -1;
            }
            proxy->buffer[proxy->buffer_l] = '\0';
        }

        bytes_to_read = proxy->buffer_sz - proxy->buffer_l - 1;

        /* look for end of header */
        char *end_of_header = strstr(proxy->buffer, "\r\n\r\n");
        if (end_of_header != NULL) {
            char *body = end_of_header + 4;
            body_bytes_read += proxy->buffer_l - (body - proxy->buffer);
            break;
        }
    }

    if (bytes_read == -1) {
        fprintf(stderr, "[Error] Proxy_read: read failed\n");
        return -1;
    }

    /* get content length */
    size_t body_length       = 0;
    char *content_length_str = strstr(proxy->buffer, "Content-Length: ");
    if (content_length_str != NULL) {
        char content_length[11];
        zero(content_length, 11);
        content_length_str += 16;
        char *end = strstr(content_length_str, "\r\n");
        if (end == NULL) {
            fprintf(stderr,
                    "[Error] Proxy_read: invalid Content-Length header\n");
            return -1;
        }
        if (end - content_length_str > 10) {
            fprintf(stderr, "[Error] Proxy_read: Content-Length too large\n");
            return -1;
        }
        memcpy(content_length, content_length_str, end - content_length_str);
        body_length = atoi(content_length);
    } else {
        body_length = 0;
    }

    /* read body */
    while (body_bytes_read < body_length) {
        bytes_read =
            read(socket, proxy->buffer + proxy->buffer_l, bytes_to_read);
        proxy->buffer_l += bytes_read;
        body_bytes_read += bytes_read;
        if (bytes_read == -1) {
            fprintf(stderr, "[Error] Proxy_read: read failed\n");
            return -1;
        }

        /* check if buffer is full */
        if (proxy->buffer_l == (proxy->buffer_sz - 1)) {
            if (expand_buffer(proxy) == -1) {
                fprintf(stderr, "[Error] Proxy_read: expand_buffer failed\n");
                return -1;
            }
            proxy->buffer[proxy->buffer_l] = '\0';
        }

        bytes_to_read = proxy->buffer_sz - proxy->buffer_l - 1;
    }

    return proxy->buffer_l;
}

int Proxy_init(struct Proxy *proxy, short port, size_t cache_size)
{
    if (proxy == NULL) {
        return -1;
    }

    /* initialize the proxy cache */
    proxy->cache = Cache_new(cache_size, Connection_free, Connection_print);
    if (proxy->cache == NULL) {
        return -1;
    }

    /* zero out the proxy addresses */
    zero(&proxy->addr, sizeof(proxy->addr));
    zero(&proxy->client_addr, sizeof(proxy->client_addr));
    zero(&proxy->server_addr, sizeof(proxy->server_addr));
    zero(&proxy->client, sizeof(proxy->client));
    zero(&proxy->server, sizeof(proxy->server));
    zero(&proxy->client_ip, sizeof(proxy->client_ip));
    zero(&proxy->server_ip, sizeof(proxy->server_ip));

    /* initialize socket fds to -1 */
    proxy->listen_fd = -1;
    proxy->server_fd = -1;
    proxy->client_fd = -1;

    /* set the proxy port */
    proxy->port = port;

    /* set buffer to NULL */
    proxy->buffer_sz = BUFSIZE;
    proxy->buffer_l  = 0;
    proxy->buffer    = calloc(proxy->buffer_sz, sizeof(char));

    /* zero out fd_sets and initialize fdmax of muliclient set */
    FD_ZERO(&master_set);   /* clear the sets */
    FD_ZERO(&readfds);
    proxy->fdmax = 0;
    proxy->timeout = NULL;

    return 0;
}

void Proxy_free(void *proxy)
{
    if (proxy == NULL) {
        return;
    }

    struct Proxy *p = (struct Proxy *)proxy;

    /* free the proxy */
    Cache_free(&p->cache);
    if (p->listen_fd != -1) {
        close(p->listen_fd);
        p->listen_fd = -1;
    }
    if (p->server_fd != -1) {
        close(p->server_fd);
        p->server_fd = -1;
    }
    if (p->client_fd != -1) {
        close(p->client_fd);
        p->client_fd = -1;
    }
    if (p->buffer != NULL) {
        free(p->buffer);
        p->buffer = NULL;
    }

    p->buffer_sz = 0;
    p->buffer_l  = 0;
    p->port      = 0;
}

void Proxy_print(struct Proxy *proxy)
{
    if (proxy == NULL) {
        return;
    }

    fprintf(stderr, "Proxy {\n");
    fprintf(stderr, "    port = %d\n", proxy->port);
    fprintf(stderr, "    listen_fd = %d\n", proxy->listen_fd);
    fprintf(stderr, "    server_fd = %d\n", proxy->server_fd);
    fprintf(stderr, "    client_fd = %d\n", proxy->client_fd);
    Cache_print(proxy->cache);
    fprintf(stderr, "}\n");
}

static int expand_buffer(struct Proxy *proxy)
{
    if (proxy == NULL) {
        return -1;
    }

    /* expand the buffer */
    proxy->buffer_sz *= 2;
    char *new_buffer = calloc(proxy->buffer_sz, sizeof(char));
    if (new_buffer == NULL) {
        return -1;
    }

    /* copy the old buffer to the new buffer */
    memcpy(new_buffer, proxy->buffer, proxy->buffer_l);

    /* free the old buffer */
    free(proxy->buffer);

    /* set the new buffer */
    proxy->buffer = new_buffer;

    return 0;
}

void Connection_init(struct Connection *conn, struct HTTP_Request *request,
                     struct HTTP_Response *response)
{
    if (conn == NULL) {
        return;
    }

    if (conn->request != NULL) {
        HTTP_free_request(conn->request);
    }

    if (conn->response != NULL) {
        HTTP_free_response(conn->response);
    }

    conn->request  = request;
    conn->response = response;

    conn->is_from_cache = false;
}

struct Connection *Connection_new(struct HTTP_Request *request,
                                  struct HTTP_Response *response)
{
    struct Connection *conn = calloc(1, sizeof(struct Connection));
    if (conn == NULL) {
        return NULL;
    }

    Connection_init(conn, request, response);

    return conn;
}

void Connection_free(void *conn)
{
    if (conn == NULL) {
        return;
    }

    struct Connection *c = (struct Connection *)conn;

    if (c->request != NULL) {
        HTTP_free_request(c->request);
    }

    if (c->response != NULL) {
        HTTP_free_response(c->response);
    }

    free(c);
}

void Connection_print(void *conn)
{
    if (conn == NULL) {
        return;
    }

    struct Connection *c = (struct Connection *)conn;

    fprintf(stderr, "Connection {\n");
    fprintf(stderr, "    %sRequest {%s\n", YEL, reset);
    HTTP_print_request(c->request);
    fprintf(stderr, "    }\n");
    fprintf(stderr, "    %sResponse {%s\n", YEL, reset);
    HTTP_print_response(c->response);
    fprintf(stderr, "    }\n");
    fprintf(stderr, "}\n");
}

static int compact_buffer(struct Proxy *proxy)
{
    if (proxy == NULL) {
        return -1;
    }

    /* compact the buffer */
    proxy->buffer_sz = proxy->buffer_l;
    proxy->buffer    = realloc(proxy->buffer, proxy->buffer_sz);
    if (proxy->buffer == NULL) {
        return -1;
    }

    return 0;
}

static void zero(void *p, size_t n)
{
    memset(p, 0, n);
}

static void clear_buffer(struct Proxy *proxy)
{
    if (proxy == NULL) {
        return;
    }

    /* clear the buffer */
    zero(proxy->buffer, proxy->buffer_sz);
    proxy->buffer_l = 0;
}


/**
 * 1. accepts connection request from new client 
 * 2. creates a client object for new client 
 * 3. puts new client's fd in master set 
 * 4. update the value of fdmax
 */
int Proxy_accept_new_client(struct Proxy *proxy)
{
    /* 1. accepts connection request from new client */
    socklen_t client_len = sizeof(proxy->client_addr);
    proxy->client_fd = accept(proxy->listen_fd, 
                              (struct sockaddr *) &(proxy->client_addr)
                              &client_len);
    if (proxy->client_fd == -1) {
        fprintf(stderr, "[Error] Proxy_accept_new_client: accept failed\n");
        // Proxy_free(proxy);
        return -1;
    }

    /** 
     * 2. Create ew client object here ...
     * ?? Client list carried by Proxy?
     */

    /* 3. puts new client's fd in master set */
    FD_SET(proxy->client_fd, &(proxy->master_set));

    /* 4. update the value of fdmax */
    proxy->fdmax = proxy->client_fd > proxy->fdmax ? 
                    proxy->client_fd : proxy->fdmax;

    return 0; // success
}