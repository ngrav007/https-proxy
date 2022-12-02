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

#define BUFFER_SZ 1024
#define NS_PER_S 1000000000.0
#define MAX_DIGITS_LONG 20

int get_char(int fd);
char *readline(int fd, size_t *len);
unsigned long hash_foo(unsigned char *str);
double get_time();
double timespec_to_double(struct timespec t);
struct timespec timespec_diff(struct timespec start, struct timespec end);
void print_ascii(char *buffer, size_t length);
char *get_buffer_lc(char *buf, char *end);
char *get_buffer(char *start, char *end);
char *remove_whitespace(char *str, int size);
void zero(void *p, size_t n);
void clear_buffer(char *buffer, size_t *buffer_l);
int expand_buffer(char **buffer, size_t *buffer_l, size_t *buffer_sz);
void free_buffer(char **buffer, size_t *buffer_l, size_t *buffer_sz);
void print_error(char *msg);

#endif
