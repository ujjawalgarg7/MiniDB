//
// Created by ujjawal on 23/08/26.
//
#include "../src/database.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREAD_COUNT 8
#define OPERATIONS 1000

typedef struct {
    HashTable *db;
    int thread_id;
} ThreadArgs;

void *expiration_worker(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;

    char key[64];
    char value[64];

    for (int i = 0; i < OPERATIONS; i++) {

        snprintf(
            key,
            sizeof(key),
            "thread_%d_key_%d",
            args->thread_id,
            i
        );

        snprintf(
            value,
            sizeof(value),
            "value_%d",
            i
        );

        /*
         * Store key with 1 second expiration.
         */
        db_set_expire(
            args->db,
            key,
            value,
            1
        );

        /*
         * Immediately check the key.
         */
        char *result =
            db_get(args->db, key);

        if (result != NULL) {
            free(result);
        }

        /*
         * Check existence.
         */
        db_exists(
            args->db,
            key
        );
    }

    printf(
        "Thread %d completed\n",
        args->thread_id
    );

    return NULL;
}

int main(void)
{
    printf("============================\n");
    printf("MiniDB Expiration Stress Test\n");
    printf("============================\n");

    printf(
        "Threads: %d\n",
        THREAD_COUNT
    );

    printf(
        "Operations per thread: %d\n",
        OPERATIONS
    );

    printf(
        "Total operations: %d\n\n",
        THREAD_COUNT * OPERATIONS
    );

    HashTable db;

    db_init(&db);

    pthread_t threads[THREAD_COUNT];

    ThreadArgs args[THREAD_COUNT];

    /*
     * Start worker threads.
     */
    for (int i = 0; i < THREAD_COUNT; i++) {

        args[i].db = &db;
        args[i].thread_id = i;

        if (pthread_create(
                &threads[i],
                NULL,
                expiration_worker,
                &args[i]
            ) != 0) {

            perror("pthread_create");
            return EXIT_FAILURE;
        }
    }

    /*
     * Wait for all threads.
     */
    for (int i = 0; i < THREAD_COUNT; i++) {

        pthread_join(
            threads[i],
            NULL
        );
    }

    printf("\nAll threads completed.\n");

    db_destroy(&db);

    printf("\nExpiration stress test complete.\n");

    return 0;
}