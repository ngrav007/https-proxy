#ifndef _UTILITY_H_
#define _UTILITY_H_

#include "colors.h"

#include <error.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define NS_PER_S 1000000000.0
#define BUFSIZE  1024

int Util_getchar(int fd);
char *Util_readline(int fd, size_t *len);
unsigned long hash_foo(unsigned char *str);
double Util_get_time();
double timespec_to_double(struct timespec t);
struct timespec timespec_diff(struct timespec start, struct timespec end);
bool Util_is_whitespace(char c);

#endif
