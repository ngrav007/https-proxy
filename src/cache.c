#include "cache.h"

static int remove_stale_entry(Cache cache);

/* ----------------------- Cache Function Definitions ----------------------- */

Cache Cache_new(size_t cap, void (*free_foo)(void *), void (*print_foo)(void *))
{
    Cache cache = calloc(1, sizeof(struct Cache));
    if (cache == NULL) {
        return NULL;
    }

    cache->table = calloc(cap, sizeof(*cache->table));
    if (cache == NULL) {
        free(cache);
        return NULL;
    }

    /* Note: uncomment to enable first-in-first-out eviction policy */
    // cache->fifo = List_new();
    // if (cache->fifo == NULL) {
    //     free(cache->table);
    //     free(cache);
    //     return NULL;
    // }

    cache->lru = List_new();
    if (cache->lru == NULL) {
        /* Note: uncomment to enable first-in-first-out eviction policy */
        // List_free(&cache->fifo);
        free(cache->table);
        free(cache);
        return NULL;
    }

    cache->capacity  = cap;
    cache->free_foo  = (free_foo == NULL) ? free : free_foo;
    cache->print_foo = print_foo;
    cache->size      = 0;

    return cache;
}

int Cache_put(Cache cache, char *key, void *value, long max_age)
{
    if (cache == NULL || key == NULL || value == NULL) {
        fprintf(stderr, "[Error] NULL parameters not allowed!\n");
        return -1;
    }

    /* if the key is not in table, create a new entry */
    Entry e = Entry_new(value, key, strlen(key), max_age);
    if (e == NULL) {
        return -1;
    }

    /* if cache is full, remove oldest entry */
    if (cache->size == cache->capacity) {
        Cache_evict(cache);
    }

    size_t i;
    for (i = 0; i < cache->capacity; i++) {
        /* Empty slot */
        if (cache->table[i] == NULL) {
            cache->table[i] = e;
            cache->size++;
            /* Note: uncomment to enable first-in-first-out eviction policy */
            // List_add_back(cache->fifo, e);
            List_add_back(cache->lru, e);
            return 0;
        } else if (cache->table[i]->deleted) {
            Entry_free(&cache->table[i], cache->free_foo);
            cache->table[i] = e;
            cache->size++;
            /* Note: uncomment to enable first-in-first-out eviction policy */
            // List_add_back(cache->fifo, e);
            List_add_back(cache->lru, e);
            return 0;
        }
    }

    return -1;
}

void *Cache_get(Cache cache, char *key)
{
    if (cache == NULL || key == NULL) {
        fprintf(stderr, "[Error] NULL parameters not allowed!\n");
        return NULL;
    }
    size_t i;
    for (i = 0; i < cache->capacity; i++) {
        Entry e = cache->table[i];
        if (e == NULL) {
            continue;
        } else if (strncmp(e->key, key, strlen(key)) == 0) { // Key Found
            /* note: uncomment to enable first-in-first-out eviction policy */
            // if (!e->retrieved) {
            // List_remove(cache->fifo, e);
            // List_add_back(cache->lru, e);
            // } else {
            List_remove(cache->lru, e);
            List_add_back(cache->lru, e);
            // }

            e->retrieved = true;
            if (e->stale) { // TODO - check if this is correct
                List_remove(cache->lru, e);
                Entry_delete(e, cache->free_foo);
                return NULL;
            }

            return e->value;
        }
    }

    return NULL;
}

Entry Cache_find(Cache cache, char *key)
{
    if (cache == NULL || key == NULL) {
        fprintf(stderr, "[Error] NULL parameters not allowed!\n");
        return NULL;
    }

    size_t i;
    for (i = 0; i < cache->capacity; i++) {
        Entry e = cache->table[i];
        if (e == NULL) {
            continue;
        } else if (strcmp(e->key, key) == 0 && !e->deleted) {
            return e;
        }
    }
    return NULL;
}

int Cache_remove(Cache cache, char *key)
{
    if (cache == NULL || key == NULL) {
        fprintf(stderr, "[Error] NULL parameters not allowed!\n");
        return -1;
    }

    Entry e = Cache_find(cache, key);
    if (e == NULL) {
        return -1;
    }

    /* Note: uncomment to enable first-in-first-out eviction policy */
    // List_remove(cache->fifo, e);
    List_remove(cache->lru, e);

    Entry_free(&e, cache->free_foo);
    cache->size--;

    return 0;
}

int Cache_evict(Cache cache)
{
    if (cache == NULL) {
        fprintf(stderr, "[Error] Null parameter given.\n");
        return -1;
    }

    /* update timestamps of all entries */
    Cache_refresh(cache);

    /* Evict Stale Entry */
    if (remove_stale_entry(cache) == 0) {
        return 0;
    }

    /* Note: uncomment to enable first-in-first-out eviction policy */
    /* Evict FIFO Entry */
    // if (cache->fifo->head != NULL) {
    //     Entry e = List_remove_front(cache->fifo);
    //     Entry_delete(e, cache->free_foo);
    //     cache->size--;
    //     return 0;
    // }

    /* Evict LRU Entry */
    if (cache->lru->head != NULL) {
        Entry e = List_remove_front(cache->lru);
        Entry_delete(e, cache->free_foo);
        cache->size--;

        return 0;
    }

    return -1; // Cache is empty
}

int Cache_refresh(Cache cache)
{
    if (cache == NULL) {
        return -1;
    }

    /* touch every entry in the cache */
    size_t i;
    for (i = 0; i < cache->capacity; i++) {
        if (cache->table[i] != NULL) {
            Entry_touch(cache->table[i]);
        }
    }

    return 0;
}

/* Cache_free
 *    Purpose: Frees the memory allocated for the Cache utilizing a given free
 *             function provided by the user of this data structure. If NULL is
 *             provided, the free function is set to the standard free function.
 * Parameters: @cache - the Cache to be freed
 *  Returns: None
 */
void Cache_free(Cache *cache)
{
    if (cache == NULL || *cache == NULL) {
        return;
    }

    /* free all entries carrying values in the cache */
    Entry curr = NULL;
    size_t i;
    for (i = 0; i < (*cache)->capacity; i++) {
        curr = (*cache)->table[i];
        if (curr != NULL) {
            Entry_free(&curr, (*cache)->free_foo);
        }
    }

    /* Note: uncomment to enable first-in-first-out eviction policy */
    // List_free(&(*cache)->fifo);
    List_free(&(*cache)->lru);

    free((*cache)->table);
    free((*cache));
    cache = NULL;
}

void Cache_print(Cache cache)
{
    if (cache == NULL) {
        return;
    }

    fprintf(stderr, "Cache {\n");
    fprintf(stderr, "    capacity = %lu\n", cache->capacity);
    fprintf(stderr, "    size = %lu\n", cache->size);
    size_t i;
    for (i = 0; i < cache->capacity; i++) {
        Entry_print(cache->table[i], cache->print_foo);
        /* Note: uncomment to enable first-in-first-out eviction policy */
        // Entry_debug_print(cache->table[i]);
    }
}

long Cache_get_age(Cache cache, char *key)
{
    if (cache == NULL) {
        return -1;
    }

    Entry e = Cache_find(cache, key);
    if (e == NULL) {
        return -1;
    }

    return Util_get_time() - e->created;
}

static int remove_stale_entry(Cache cache)
{
    if (cache == NULL) {
        return -1;
    }
    Entry oldest = NULL;

    /* find oldest stale entry */
    size_t i;
    for (i = 0; i < cache->capacity; i++) {
        if (cache->table[i] != NULL && cache->table[i]->stale) {
            if (oldest == NULL || Entry_is_older(cache->table[i], oldest)) {
                oldest = cache->table[i];
            }
        }
    }

    if (oldest == NULL) {
        return -1;
    }

    /* remove oldest entry from fifo and lru lists */
    /* Note: uncomment to enable first-in-first-out eviction policy */
    // List_remove(cache->fifo, oldest);
    List_remove(cache->lru, oldest);

    /* remove oldest stale entry */
    Entry_delete(oldest, cache->free_foo);
    cache->size--;

    return 0;
}
