#ifndef _UTILITY_H_
#define _UTILITY_H_

#include "colors.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// #if RUN_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
// #endif 

#define BUFFER_SZ 1024
#define NS_PER_S 1000000000.0
#define US_PER_S 1000000.0
#define MAX_DIGITS_LONG 20
#define TRUE  1
#define FALSE 0

int get_char(int fd);
char *readline(int fd, size_t *len);
unsigned long hash_foo(unsigned char *str);
double get_current_time();
double timespec_to_double(struct timespec t);
double timeval_to_double(struct timeval t);
void double_to_timeval(struct timeval *t, double d);
struct timespec timespec_diff(struct timespec start, struct timespec end);
void print_ascii(char *buffer, size_t length);
char *get_buffer_lc(char *buf, char *end);
char *get_buffer(char *start, char *end);
char *remove_whitespace(char *str, int size);
int expand_buffer(char **buffer, size_t *buffer_l, size_t *buffer_sz);
void zero(void *p, size_t n);
void clear_buffer(char *buffer, size_t *buffer_l);
void free_buffer(char **buffer, size_t *buffer_l, size_t *buffer_sz);
void print_error(char *msg);
void print_success(char *msg);
void print_info(char *msg);
void print_warning(char *msg);
void print_debug(char *msg);

// #if RUN_SSL
int LoadClientCertificates(SSL_CTX *ctx, char *cert_file, char *key_file); // , char *passwd);
int LoadCertificates(SSL_CTX *ctx, char *cert_file, char *key_file); // , char *passwd);
SSL_CTX *InitServerCTX();
SSL_CTX *InitCTX();
void ShowCerts(SSL *ssl);

int is_root();
// #endif

#endif
