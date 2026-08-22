#ifndef DATABASE_H
#define DATABASE_H
#include <pthread.h>
#include <time.h>
#define TABLE_SIZE 10

typedef struct Entry {
    char *key;
    char *value;

    time_t expires_at;

    struct Entry *next;
} Entry;


typedef struct {
    Entry *buckets[TABLE_SIZE];
    pthread_rwlock_t locks[TABLE_SIZE];

    pthread_t expiration_thread;
    int expiration_thread_running;

    pthread_mutex_t expiration_mutex;
    pthread_cond_t expiration_cond;
} HashTable;


void db_init(HashTable *db);

void db_set(
    HashTable *db,
    const char *key,
    const char *value
);

char *db_get(
    HashTable *db,
    const char *key
);

int db_delete(
    HashTable *db,
    const char *key
);

int db_exists(
    HashTable *db,
    const char *key
);

void db_destroy(
    HashTable *db
);

void db_set_expire(
    HashTable *db,
    const char *key,
    const char *value,
    int seconds
);

#endif