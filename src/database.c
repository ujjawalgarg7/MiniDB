#include "database.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


static char *duplicate_string(const char *string)
{
    char *copy = malloc(strlen(string) + 1);

    if (copy == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    strcpy(copy, string);

    return copy;
}


static unsigned int hash_key(const char *key)
{
    unsigned int hash = 0;

    while (*key != '\0') {
        hash += (unsigned char)*key;
        key++;
    }

    return hash % TABLE_SIZE;
}


/*
 * Caller must hold the bucket write lock.
 */
static void remove_expired_entries(
    HashTable *db,
    unsigned int bucket
)
{
    time_t now = time(NULL);

    Entry *current = db->buckets[bucket];
    Entry *previous = NULL;

    while (current != NULL) {

        Entry *next = current->next;

        if (
            current->expires_at != 0 &&
            current->expires_at <= now
        ) {

            if (previous == NULL) {
                db->buckets[bucket] = next;
            } else {
                previous->next = next;
            }

            free(current->key);
            free(current->value);
            free(current);

        } else {
            previous = current;
        }

        current = next;
    }
}


/*
 * Background expiration worker.
 */
static void *expiration_worker(void *arg)
{
    HashTable *db = arg;

    while (1) {

        pthread_mutex_lock(
            &db->expiration_mutex
        );

        if (!db->expiration_thread_running) {

            pthread_mutex_unlock(
                &db->expiration_mutex
            );

            break;
        }


        /*
         * Wake approximately every 100 ms.
         */
        struct timespec deadline;

        clock_gettime(
            CLOCK_REALTIME,
            &deadline
        );

        deadline.tv_nsec += 100000000L;

        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000L;
        }

        pthread_cond_timedwait(
            &db->expiration_cond,
            &db->expiration_mutex,
            &deadline
        );


        int running =
            db->expiration_thread_running;

        pthread_mutex_unlock(
            &db->expiration_mutex
        );


        if (!running) {
            break;
        }


        /*
         * Scan all buckets.
         */
        for (unsigned int i = 0;
             i < TABLE_SIZE;
             i++) {

            pthread_rwlock_wrlock(
                &db->locks[i]
            );

            remove_expired_entries(
                db,
                i
            );

            pthread_rwlock_unlock(
                &db->locks[i]
            );
        }
    }

    return NULL;
}


void db_init(HashTable *db)
{
    for (unsigned int i = 0;
         i < TABLE_SIZE;
         i++) {

        db->buckets[i] = NULL;

        pthread_rwlock_init(
            &db->locks[i],
            NULL
        );
    }


    pthread_mutex_init(
        &db->expiration_mutex,
        NULL
    );

    pthread_cond_init(
        &db->expiration_cond,
        NULL
    );


    db->expiration_thread_running = 1;


    if (pthread_create(
            &db->expiration_thread,
            NULL,
            expiration_worker,
            db
        ) != 0) {

        perror("pthread_create");

        exit(EXIT_FAILURE);
    }
}


void db_set(
    HashTable *db,
    const char *key,
    const char *value
)
{
    db_set_expires_at(
        db,
        key,
        value,
        0
    );
}


void db_set_expires_at(
    HashTable *db,
    const char *key,
    const char *value,
    time_t expires_at
)
{
    unsigned int hash =
        hash_key(key);


    pthread_rwlock_wrlock(
        &db->locks[hash]
    );


    /*
     * Remove expired version first.
     */
    remove_expired_entries(
        db,
        hash
    );


    Entry *current =
        db->buckets[hash];


    while (current != NULL) {

        if (strcmp(
                current->key,
                key
            ) == 0) {

            free(current->value);

            current->value =
                duplicate_string(value);

            current->expires_at =
                expires_at;


            pthread_rwlock_unlock(
                &db->locks[hash]
            );

            return;
        }

        current = current->next;
    }


    Entry *new_entry =
        malloc(sizeof(Entry));

    if (new_entry == NULL) {

        perror("malloc");

        pthread_rwlock_unlock(
            &db->locks[hash]
        );

        exit(EXIT_FAILURE);
    }


    new_entry->key =
        duplicate_string(key);

    new_entry->value =
        duplicate_string(value);

    new_entry->expires_at =
        expires_at;

    new_entry->next =
        db->buckets[hash];

    db->buckets[hash] =
        new_entry;


    pthread_rwlock_unlock(
        &db->locks[hash]
    );


    /*
     * Wake expiration worker.
     */
    pthread_mutex_lock(
        &db->expiration_mutex
    );

    pthread_cond_signal(
        &db->expiration_cond
    );

    pthread_mutex_unlock(
        &db->expiration_mutex
    );
}


char *db_get(
    HashTable *db,
    const char *key
)
{
    unsigned int hash =
        hash_key(key);


    /*
     * Write lock because GET can remove
     * an expired entry.
     */
    pthread_rwlock_wrlock(
        &db->locks[hash]
    );


    remove_expired_entries(
        db,
        hash
    );


    Entry *current =
        db->buckets[hash];


    while (current != NULL) {

        if (strcmp(
                current->key,
                key
            ) == 0) {

            char *value =
                duplicate_string(
                    current->value
                );

            pthread_rwlock_unlock(
                &db->locks[hash]
            );

            return value;
        }

        current = current->next;
    }


    pthread_rwlock_unlock(
        &db->locks[hash]
    );

    return NULL;
}


int db_delete(
    HashTable *db,
    const char *key
)
{
    unsigned int hash =
        hash_key(key);


    pthread_rwlock_wrlock(
        &db->locks[hash]
    );


    remove_expired_entries(
        db,
        hash
    );


    Entry *current =
        db->buckets[hash];

    Entry *previous = NULL;


    while (current != NULL) {

        if (strcmp(
                current->key,
                key
            ) == 0) {

            if (previous == NULL) {
                db->buckets[hash] =
                    current->next;
            } else {
                previous->next =
                    current->next;
            }


            free(current->key);
            free(current->value);
            free(current);


            pthread_rwlock_unlock(
                &db->locks[hash]
            );

            return 1;
        }


        previous = current;
        current = current->next;
    }


    pthread_rwlock_unlock(
        &db->locks[hash]
    );

    return 0;
}


int db_exists(
    HashTable *db,
    const char *key
)
{
    unsigned int hash =
        hash_key(key);


    pthread_rwlock_wrlock(
        &db->locks[hash]
    );


    remove_expired_entries(
        db,
        hash
    );


    Entry *current =
        db->buckets[hash];


    while (current != NULL) {

        if (strcmp(
                current->key,
                key
            ) == 0) {

            pthread_rwlock_unlock(
                &db->locks[hash]
            );

            return 1;
        }

        current = current->next;
    }


    pthread_rwlock_unlock(
        &db->locks[hash]
    );

    return 0;
}


void db_destroy(HashTable *db)
{
    /*
     * Stop expiration thread.
     */
    pthread_mutex_lock(
        &db->expiration_mutex
    );

    db->expiration_thread_running = 0;

    pthread_cond_signal(
        &db->expiration_cond
    );

    pthread_mutex_unlock(
        &db->expiration_mutex
    );


    pthread_join(
        db->expiration_thread,
        NULL
    );


    /*
     * Free all entries.
     */
    for (unsigned int i = 0;
         i < TABLE_SIZE;
         i++) {

        pthread_rwlock_wrlock(
            &db->locks[i]
        );


        Entry *current =
            db->buckets[i];


        while (current != NULL) {

            Entry *next =
                current->next;

            free(current->key);
            free(current->value);
            free(current);

            current = next;
        }


        db->buckets[i] = NULL;


        pthread_rwlock_unlock(
            &db->locks[i]
        );


        pthread_rwlock_destroy(
            &db->locks[i]
        );
    }


    pthread_mutex_destroy(
        &db->expiration_mutex
    );

    pthread_cond_destroy(
        &db->expiration_cond
    );
}