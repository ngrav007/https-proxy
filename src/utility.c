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
    for (size_t i = 0; i < len; i++) {
        if (isprint(buf[i])) {
            printf("%c", buf[i]);
        } else {
            printf(".");
        }
    }
    printf("\n");
}
