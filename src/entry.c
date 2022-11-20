#include "entry.h"

Entry *Entry_new(void *value, void *key, size_t key_l, long max_age)
{
    Entry *entry = calloc(1, sizeof(struct Entry));
    if (entry == NULL) {
        return NULL;
    }

    entry->value = value;
    memcpy(entry->key, key, key_l);
    entry->key_l     = key_l;
    entry->max_age   = max_age;
    entry->ttl       = max_age;
    entry->stale     = (entry->ttl <= 0) ? true : false;
    entry->deleted   = false;
    entry->retrieved = RETRIEVED_VALUE;
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    entry->init_time = timespec_to_double(now);

    return entry;
}

void Entry_init(Entry *entry, char *key, void *value, long max_age)
{
    if (entry == NULL) {
        entry = Entry_new(value, key, strlen(key), max_age);
    } else {
        entry->value = value;
        entry->key_l = strlen(key);
        strncpy(entry->key, key, entry->key_l);
        entry->max_age = max_age;
        entry->ttl     = max_age;
        entry->stale     = false;
        entry->deleted   = false;
        entry->retrieved = RETRIEVED_VALUE;
    }
}

void Entry_free(Entry **entry, void (*foo)(void *))
{
    if (entry == NULL || *entry == NULL) {
        return;
    }

    if (foo == NULL) {
        foo = free;
    }

    foo((*entry)->value);
    (*entry)->value = NULL;
    memset((*entry)->key, 0, PATH_MAX);
    (*entry)->key_l   = 0;
    (*entry)->max_age = 0;
    (*entry)->ttl     = 0;
    (*entry)->stale   = false;
    (*entry)->deleted = true;
    free((*entry));
    (*entry) = NULL;
}

void Entry_delete(Entry *entry, void (*foo)(void *))
{
    if (entry == NULL) {
        return;
    }
    entry->deleted = true;
    if (foo == NULL) {
        foo = free;
    }
    
    foo(entry->value);
    entry->value = NULL;
    memset(entry->key, 0, PATH_MAX);
    entry->key_l   = 0;
    entry->max_age = 0;
    entry->ttl     = 0;
    entry->deleted = true;
}

void Entry_print(Entry *entry, void (*foo)(void *))
{
    if (entry == NULL) {
        return;
    }
    fprintf(stderr, "Entry {\n");
    if (foo == NULL) {
        fprintf(stderr, "    value = %p\n", entry->value);
    } else {
        fprintf(stderr, "    value =\n");
        foo(entry->value);
    }
    fprintf(stderr, "    key = %s\n", entry->key);
    fprintf(stderr, "    init_time = %f\n", entry->init_time);
    fprintf(stderr, "    max_age = %f\n", entry->max_age);
    fprintf(stderr, "    ttl = %f\n", entry->ttl);
    fprintf(stderr, "    stale = %d\n", entry->stale);
    fprintf(stderr, "    deleted = %d\n", entry->deleted);
    fprintf(stderr, "}\n");
}

int Entry_update(Entry *entry, void *value, long max_age, void (*foo)(void *))
{
    if (entry == NULL) {
        return -1;
    }
    if (foo == NULL) {
        foo = free;
    }

    if (entry->value != NULL) {
        foo(entry->value); // Free old value
    }

    entry->value   = value;
    entry->max_age = max_age;
    entry->ttl     = max_age;
    entry->init_time = get_time();
    entry->stale   = (entry->ttl <= 0) ? true : false;
    entry->deleted = false;

    return 0;
}

long Entry_get_ttl(Entry *entry) { return entry->ttl; }

/* Updates the entry's time to live and sets the stale flag if stale  */
int Entry_touch(Entry *entry)
{
    if (entry == NULL) {
        return -1;
    }

    double now = get_time();
    double age = now - entry->init_time;
    entry->ttl = entry->max_age - age;

    if (entry->ttl <= 0) {
        entry->stale = true;
        return 1;
    } else {
        entry->stale = false;
    }

    return 0;
}

long Entry_get_age(Entry *entry)
{
    if (entry == NULL) {
        return -1;
    }

    double now = get_time();
    double age = now - entry->init_time;

    return (long)age;
}

/* Boolean function that returns true if the entry is stale */
bool Entry_is_stale(Entry *entry)
{
    if (entry == NULL) {
        return false;
    }
    return entry->stale;
}

/* Is entry 1 older than entry 2? */
bool Entry_is_older(Entry *entry1, Entry *entry2)
{
    if (entry1 == NULL || entry2 == NULL) {
        return false;
    }
    return entry1->init_time < entry2->init_time;
}

bool Entry_is_equal(Entry *entry, char *key)
{
    if (entry == NULL || key == NULL) {
        return false;
    }

    if (entry->key_l != strlen(key)) {
        return false;
    }

    return strncmp(entry->key, key, entry->key_l) == 0;
}

bool Entry_is_empty(Entry *entry) { return (entry->value == NULL); }

bool Entry_is_deleted(Entry *entry) { return entry->deleted; }

void Entry_debug_print(Entry *entry)
{
    if (entry == NULL) {
        return;
    }
    fprintf(stderr, "Entry {\n");
    fprintf(stderr, "    key = %s\n", entry->key);
    fprintf(stderr, "    init_time = %f\n", entry->init_time);
    fprintf(stderr, "    max_age = %f\n", entry->max_age);
    fprintf(stderr, "    ttl = %f\n", entry->ttl);
    fprintf(stderr, "    stale = %d\n", entry->stale);
    fprintf(stderr, "    deleted = %d\n", entry->deleted);
    fprintf(stderr, "    age = %ld\n", Entry_get_age(entry));
    fprintf(stderr, "}\n");
}
