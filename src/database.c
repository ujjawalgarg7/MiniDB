#include "database.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *duplicate_string(const char *string) {
    char *copy = malloc(strlen(string) + 1);

    if (copy == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    strcpy(copy, string);
    return copy;
}

static unsigned int hash_key(const char *key) {
    unsigned int hash = 0;
    while (*key != '\0') {
        hash += (unsigned char)*key;
        key++;
    }

    return hash % TABLE_SIZE;
}

void db_init(HashTable *db) {
    for (unsigned int i = 0; i < TABLE_SIZE; i++) {
        db->buckets[i] = NULL;
    }
    pthread_mutex_init(&db->lock, NULL);
}


void db_set(HashTable *db,const char *key,const char *value) {
    pthread_mutex_lock(&db->lock);

    unsigned int hash = hash_key(key);

    Entry *current = db->buckets[hash];

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            free(current->value);
            current->value = duplicate_string(value);
            pthread_mutex_unlock(&db->lock);

            return;
        }
        current = current->next;
    }

    Entry *new_entry = malloc(sizeof(Entry));

    if (new_entry == NULL) {
        perror("malloc");
        pthread_mutex_unlock(&db->lock);
        exit(EXIT_FAILURE);
    }

    new_entry->key = duplicate_string(key);
    new_entry->value = duplicate_string(value);

    new_entry->next = db->buckets[hash];
    db->buckets[hash] = new_entry;
    pthread_mutex_unlock(&db->lock);
}


char *db_get(HashTable *db, const char *key) {
    pthread_mutex_lock(&db->lock);
    unsigned int hash = hash_key(key);
    Entry *current = db->buckets[hash];

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {

            char *value = duplicate_string(current->value);
            pthread_mutex_unlock(&db->lock);
            return value;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&db->lock);
    return NULL;
}

int db_delete(HashTable *db, const char *key) {
    pthread_mutex_lock(&db->lock);

    unsigned int hash = hash_key(key);

    Entry *current = db->buckets[hash];
    Entry *previous = NULL;

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            if (previous == NULL) {
                db->buckets[hash] = current->next;
            }else {
                previous->next = current->next;
            }
            free(current->key);
            free(current->value);
            free(current);

            pthread_mutex_unlock(&db->lock);
            return 1;
        }
        previous = current;
        current = current->next;
    }
    pthread_mutex_unlock(&db->lock);
    return 0;
}

int db_exists(HashTable *db, const char *key) {
    pthread_mutex_lock(&db->lock);
    unsigned int hash = hash_key(key);
    Entry *current = db->buckets[hash];

    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            pthread_mutex_unlock(&db->lock);
            return 1;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&db->lock);
    return 0;
}

void db_destroy(HashTable *db) {
    pthread_mutex_lock(&db->lock);

    for (unsigned int i = 0; i < TABLE_SIZE; i++) {
        Entry *current = db->buckets[i];

        while (current != NULL) {
            Entry *next = current->next;
            free(current->key);
            free(current->value);
            free(current);

            current = next;
        }
        db->buckets[i] = NULL;
    }

    pthread_mutex_unlock(&db->lock);
    pthread_mutex_destroy(&db->lock);
}




















