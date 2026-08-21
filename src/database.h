//
// Created by ujjawal on 19/08/26.
//

#ifndef MINIDB_DATABASE_H
#define MINIDB_DATABASE_H
#include <pthread.h>

#define TABLE_SIZE 10

typedef struct Entry {
    char *key;
    char *value;
    struct Entry *next;
} Entry;

typedef struct {
    Entry *buckets[TABLE_SIZE];

    pthread_mutex_t lock;
} HashTable;

void db_init(HashTable *db);

void db_set(HashTable *db, char *key, char *value);
char *db_get(HashTable *db, char *key);
int db_exists(HashTable *db, char *key);
void db_destroy(HashTable *db);

#endif //MINIDB_DATABASE_H
