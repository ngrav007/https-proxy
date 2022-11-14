#ifndef _CACHE_H_
#define _CACHE_H_

#include "colors.h"
#include "config.h"
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
    List *lru;

    size_t capacity;
    size_t size;
    void (*free_foo)(void *);
    void (*print_foo)(void *);
} Cache;

Cache *Cache_new(size_t cap, void (*foo)(void *), void (*print_foo)(void *));
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

#endif /* _CACHE_H_ */
