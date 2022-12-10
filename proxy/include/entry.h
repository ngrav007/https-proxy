#ifndef _ENTRY_H_
#define _ENTRY_H_

#include "colors.h"
#include "config.h"
#include "utility.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define RETRIEVED_VALUE true

typedef struct Entry {
    void *value;
    char key[PATH_MAX + HOST_NAME_MAX + 1];
    size_t key_l;
    double init_time;     // time this entry was created
    double max_age;
    double ttl;

    bool stale;
    bool deleted;
    bool retrieved;
} Entry;

Entry *Entry_new(void *value, void *key, size_t key_l, long max_age);
void Entry_init(Entry *entry, char *key, void *value, long max_age);
void Entry_free(Entry **entry, void (*foo)(void *));
void Entry_delete(void *entry, void (*foo)(void *));
int Entry_cmp(void *entry1, void *entry2);
int Entry_touch(Entry *entry);
void Entry_print(void *entry, void (*foo)(void *));
int Entry_update(Entry *entry, void *value, long ttl, void (*foo)(void *));
long Entry_get_age(Entry *entry);
long Entry_get_ttl(Entry *entry);
bool Entry_is_stale(Entry *entry);
bool Entry_is_older(Entry *entry1, Entry *entry2);
bool Entry_is_equal(Entry *entry1, char *key);
bool Entry_is_empty(Entry *entry);
bool Entry_is_deleted(Entry *entry);
void Entry_debug_print(Entry *entry);

#endif /* _ENTRY_H_ */
