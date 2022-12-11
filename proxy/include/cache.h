#ifndef _CACHE_H_
#define _CACHE_H_

#include "config.h"
#include "colors.h"
#include "entry.h"
#include "http.h"
#include "list.h"
#include "node.h"
#include "utility.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct Cache {
    Entry **table; 
    List *lru; /* Node data is Entry * */

    size_t capacity;
    size_t size;
    void (*free_foo)(void *);
    void (*print_foo)(void *);
    int (*cmp_foo)(void *, void *);

    char *key_array[CACHE_SZ];
} Cache;

Cache *Cache_new(size_t cap, void (*free_foo)(void *), void (*print_foo)(void *));
void Cache_free(Cache **cache);
void Cache_print(Cache *cache);
int Cache_put(Cache *cache, char *key, void *value, long max_age);
int Cache_evict(Cache *cache);
void *Cache_get(Cache *cache, char *key);
int Cache_refresh(Cache *cache);
Entry *Cache_find(Cache *cache, char *key);
long Cache_get_age(Cache *cache, char *key);
int Cache_remove(Cache *cache, char *key);
int Cache_delete(Cache *cache, char *key);

/* returns pointer to start of string array of cache keys: key_array */
char **Cache_getKeyList(Cache *cache);

#endif /* _CACHE_H_ */
