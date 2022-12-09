#include "utility.h"


int get_char(int fd)
{
    char c;
    int n = read(fd, &c, 1);
    if (n == 1) {
        return c;
    }
    return EOF;
}

char *readline(int fd, size_t *len)
{
    char *line = NULL;
    size_t n   = 0;
    int c;
    while ((c = get_char(fd)) != EOF) {
        line      = realloc(line, n + 1);
        line[n++] = c;

        if (c == '\n') {
            line[n - 1] = '\0';
            *len        = n;
            break;
        }
    }

    if (n > 0) {
        if (c == EOF) {
            line    = realloc(line, n + 1);
            line[n] = '\0';
        } else {
            line[n - 1] = '\0';
        }
        *len = n;
    } else {
        free(line);
        line = NULL;
    }

    return line;
}

/* returns a malloc'd string copy of buffer where letters are all lowercase 
   up to but not including end. */
char *get_buffer_lc(char *buf, char *end) 
{
    int size = end - buf;
    char *copy = malloc(size + 1);
    copy[size] = '\0';

    char *i;
    int j = 0;
    for (i = buf, j = 0; i != end; i++, j++) {
        if (isalpha(*i)) {
            copy[j] = tolower(*i);
        } else {
            copy[j] = *i;
        }
    }

    return copy;
}

/* hash_foo
 * @brief: djb2 - a simple hash function for strings, developed by Dan Bernstein
 *         (http://www.cse.yorku.ca/~oz/hash.html)
 * @param: str - the string to hash
 * @return: the hash value of the string
 */
unsigned long hash_foo(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

double get_time(void)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return timespec_to_double(now);
}

double timespec_to_double(struct timespec t)
{
    double d = t.tv_sec;
    d += t.tv_nsec / NS_PER_S;
    return d;
}

struct timespec timespec_diff(struct timespec start, struct timespec end)
{
    struct timespec diff;
    diff.tv_sec  = end.tv_sec - start.tv_sec;
    diff.tv_nsec = end.tv_nsec - start.tv_nsec;
    if (diff.tv_nsec < 0) {
        diff.tv_sec--;
        diff.tv_nsec += NS_PER_S;
    }
    return diff;
}

void print_ascii(char *buf, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        if (isprint(buf[i])) {
            fprintf(stderr, "%c", buf[i]);
        } else if (buf[i] == '\r') {
            fprintf(stderr, "\r");
        } else if (buf[i] == '\n') {
            fprintf(stderr, "\n");
        } else {
            fprintf(stderr, ".");
        }
    }
    fprintf(stderr, "\n");
}

/* returns a malloc'd string copy of buffer 
   up to but not including end. */
char *get_buffer(char *start, char *end)
{
    int size = end - start;
    char *copy = calloc(size + 1, sizeof(char));
    // copy[size] = '\0';
    memcpy(copy, start, size);

    return copy;
}

/**
 * Removes space characters in a given string, and returns the modified string.
 */
char *remove_whitespace(char *str, int size)
{
    unsigned int strIdx = 0;
    int i = 0;
    for (i = 0; i < size; i++) {
        if (str[i] != ' ') {
            str[strIdx] = str[i];
            strIdx++;
        }
    }
    str[strIdx] = '\0';
    return str;
}

void clear_buffer(char *buffer, size_t *buffer_l)
{
    if (buffer == NULL || buffer_l == NULL || *buffer_l == 0) {
        return;
    }

    /* clear the buffer */
    zero(buffer, *buffer_l);
    *buffer_l = 0;
}

int expand_buffer(char **buffer, size_t *buffer_l, size_t *buffer_sz)
{
    if (buffer == NULL || *buffer == NULL || buffer_l == NULL || buffer_sz == NULL) {
        return -1;
    }

    /* expand the buffer */
   *buffer_sz += BUFFER_SZ;
    char *new_buffer = calloc(*buffer_sz + 1, sizeof(char));
    if (new_buffer == NULL) {
        return -1;
    }

    /* copy the old buffer to the new buffer */
    memcpy(new_buffer, *buffer, *buffer_l);

    /* free the old buffer */
    free(*buffer);

    /* set the new buffer */
    *buffer = new_buffer;

    return 0;
}

void free_buffer(char **buffer, size_t *buffer_l, size_t *buffer_sz)
{
    if (buffer == NULL || *buffer == NULL) {
        return;
    }

    /* free the buffer */
    free(*buffer);

    /* set the buffer to NULL */
    *buffer = NULL;

    if (buffer_l != NULL) {
        *buffer_l = 0;
    }

    if (buffer_sz != NULL) {
        *buffer_sz = 0;
    }
}

void zero(void *p, size_t n)
{
    if (p == NULL) {
        return;
    }

    memset(p, 0, n);
}

void print_error(char *msg)
{
    fprintf(stderr, "%s[!]%s %s\n", RED, reset, msg);
}

void print_success(char *msg)
{
    fprintf(stderr, "%s[+]%s %s\n", GRN, reset, msg);
}

void print_info(char *msg)
{
    fprintf(stderr, "%s[*]%s %s\n", BLU, reset, msg);
}

void print_warning(char *msg)
{
    fprintf(stderr, "%s[!]%s %s\n", YEL, reset, msg);
}

void print_debug(char *msg)
{
    fprintf(stderr, "%s[DEBUG]%s %s\n", BYEL, reset, msg);
}

SSL_CTX *init_server_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    method = TLS_server_method();
    ctx = SSL_CTX_new(method);
    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    return ctx;
}

SSL_CTX *init_client_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    method = TLS_client_method();
    ctx = SSL_CTX_new(method);
    if (ctx == NULL) {
        fprintf(stderr, "[Proxy_SSL_connect]: failed to create SSL_CTX to connect to destination host\n");
        ERR_print_errors_fp(stderr);
        return NULL; /* should close the client since we can't connect to server */
    }

    return ctx;
}

int load_ca_certificates(SSL_CTX *ctk, char *ca_cert_file, char *ca_key_file)
{
    /* set the key and the certificate */
    if (SSL_CTX_use_certificate_file(ctk, ca_cert_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    /* set the private key from the key file - can be same as CertFile */
    if (SSL_CTX_use_PrivateKey_file(ctk, ca_key_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    /* verify the private key */
    if (!SSL_CTX_check_private_key(ctk)) {
        print_error("Private key does not match the certificate public key");
        return -1;
    }

    return 0;
}

