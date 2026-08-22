#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "../src/database.h"

#include <time.h>
#define NUM_THREADS 8
#define OPERATIONS_PER_THREAD 10000


typedef struct {
    HashTable *db;
    int thread_id;
} WorkerArgs;


void *stress_worker(void *arg)
{
    WorkerArgs *args = (WorkerArgs *)arg;

    HashTable *db = args->db;
    int thread_id = args->thread_id;


    char key[64];
    char value[64];


    for (int i = 0;
         i < OPERATIONS_PER_THREAD;
         i++) {

        /*
         * Each thread works on its
         * own key.
         */
        snprintf(
            key,
            sizeof(key),
            "thread_%d",
            thread_id
        );


        snprintf(
            value,
            sizeof(value),
            "value_%d",
            i
        );


        /*
         * SET
         */
        db_set(
            db,
            key,
            value
        );


        /*
         * GET
         */
        char *result =
            db_get(
                db,
                key
            );


        if (result == NULL) {

            printf(
                "ERROR: Thread %d "
                "GET returned NULL\n",
                thread_id
            );

            continue;
        }


        /*
         * We don't need the value anymore.
         */
        free(result);


        /*
         * EXISTS
         */
        int exists =
            db_exists(
                db,
                key
            );


        if (!exists) {

            printf(
                "ERROR: Thread %d "
                "key does not exist\n",
                thread_id
            );
        }
    }


    printf(
        "Thread %d completed\n",
        thread_id
    );


    return NULL;
}


int main(void)
{
    printf(
        "============================\n"
        "MiniDB Concurrency Stress Test\n"
        "============================\n"
    );


    HashTable db;

    db_init(&db);


    pthread_t threads[NUM_THREADS];

    WorkerArgs args[NUM_THREADS];


    printf(
        "Threads: %d\n",
        NUM_THREADS
    );

    printf(
        "Operations per thread: %d\n",
        OPERATIONS_PER_THREAD
    );

    printf(
        "Total operations: %d\n\n",
        NUM_THREADS *
        OPERATIONS_PER_THREAD
    );

    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    /*
     * Start worker threads.
     */
    for (int i = 0;
         i < NUM_THREADS;
         i++) {

        args[i].db = &db;
        args[i].thread_id = i;


        if (pthread_create(
                &threads[i],
                NULL,
                stress_worker,
                &args[i]
            ) != 0) {

            perror(
                "pthread_create"
            );

            db_destroy(&db);

            return 1;
        }
    }


    /*
     * Wait for all threads.
     */
    for (int i = 0;
         i < NUM_THREADS;
         i++) {

        pthread_join(
            threads[i],
            NULL
        );
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed =
    (end.tv_sec - start.tv_sec)
    +
    (end.tv_nsec - start.tv_nsec) / 1e9;

    long total_operations =
        (long) NUM_THREADS *
        OPERATIONS_PER_THREAD;

    double throughput =
        total_operations / elapsed;


    printf(
    "\nExecution time: %.6f seconds\n",
    elapsed
);

    printf(
        "Throughput: %.2f operations/second\n",
        throughput
    );

    printf(
        "\nAll threads completed.\n"
    );


    /*
     * Verify final database state.
     */
    printf(
        "\nFinal verification:\n"
    );


    for (int i = 0;
         i < NUM_THREADS;
         i++) {

        char key[64];

        snprintf(
            key,
            sizeof(key),
            "thread_%d",
            i
        );


        char *value =
            db_get(
                &db,
                key
            );


        if (value != NULL) {

            printf(
                "  %s = %s\n",
                key,
                value
            );

            free(value);

        } else {

            printf(
                "  %s = MISSING\n",
                key
            );
        }
    }


    db_destroy(&db);


    printf(
        "\nStress test complete.\n"
    );


    return 0;
}