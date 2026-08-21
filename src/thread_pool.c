//
// Created by ujjawal on 21/08/26.
//
#include "thread_pool.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>

static void client_handler(int client_fd,HashTable *db){
    char buffer[1024];


    printf(
        "Client handler started: fd=%d\n",
        client_fd
    );


    while (1) {

        ssize_t bytes_received = recv(
            client_fd,
            buffer,
            sizeof(buffer) - 1,
            0
        );


        if (bytes_received < 0) {

            perror("recv");

            break;
        }


        if (bytes_received == 0) {

            printf(
                "Client disconnected: fd=%d\n",
                client_fd
            );

            break;
        }


        buffer[bytes_received] = '\0';


        /*
         * Remove newline.
         */
        buffer[strcspn(
            buffer,
            "\r\n"
        )] = '\0';


        /*
         * Parse command.
         */
        char *command = strtok(
            buffer,
            " "
        );


        if (command == NULL) {
            continue;
        }


        /*
         * SET
         */
        if (strcmp(command, "SET") == 0) {

            char *key = strtok(NULL, " ");
            char *value = strtok(NULL, " ");


            if (key == NULL || value == NULL) {

                send(
                    client_fd,
                    "ERR usage: SET key value\n",
                    25,
                    0
                );

                continue;
            }


            db_set(
                db,
                key,
                value
            );


            send(
                client_fd,
                "OK\n",
                3,
                0
            );
        }


        /*
         * GET
         */
        else if (strcmp(command, "GET") == 0) {

            char *key = strtok(
                NULL,
                " "
            );


            if (key == NULL) {

                send(
                    client_fd,
                    "ERR usage: GET key\n",
                    21,
                    0
                );

                continue;
            }


            char *value = db_get(
                db,
                key
            );


            if (value == NULL) {

                send(
                    client_fd,
                    "(nil)\n",
                    6,
                    0
                );

            } else {

                char response[1024];


                snprintf(
                    response,
                    sizeof(response),
                    "%s\n",
                    value
                );


                send(
                    client_fd,
                    response,
                    strlen(response),
                    0
                );


                free(value);
            }
        }


        /*
         * DEL
         */
        else if (strcmp(command, "DEL") == 0) {

            char *key = strtok(
                NULL,
                " "
            );


            if (key == NULL) {

                send(
                    client_fd,
                    "ERR usage: DEL key\n",
                    21,
                    0
                );

                continue;
            }


            int deleted = db_delete(
                db,
                key
            );


            char response[32];


            snprintf(
                response,
                sizeof(response),
                "%d\n",
                deleted
            );


            send(
                client_fd,
                response,
                strlen(response),
                0
            );
        }


        /*
         * EXISTS
         */
        else if (strcmp(command, "EXISTS") == 0) {

            char *key = strtok(
                NULL,
                " "
            );


            if (key == NULL) {

                send(
                    client_fd,
                    "ERR usage: EXISTS key\n",
                    24,
                    0
                );

                continue;
            }


            int exists = db_exists(
                db,
                key
            );


            char response[32];


            snprintf(
                response,
                sizeof(response),
                "%d\n",
                exists
            );


            send(
                client_fd,
                response,
                strlen(response),
                0
            );
        }


        /*
         * EXIT
         */
        else if (strcmp(command, "EXIT") == 0) {

            send(
                client_fd,
                "BYE\n",
                4,
                0
            );

            break;
        }


        /*
         * Unknown command
         */
        else {

            send(
                client_fd,
                "ERR unknown command\n",
                21,
                0
            );
        }
    }


    close(client_fd);
}

static void *worker_function(void *arg){

    ThreadPool *pool = (ThreadPool *)arg;


    while (1) {

        pthread_mutex_lock(
            &pool->mutex
        );


        /*
         * Wait until work exists.
         */
        while (
            pool->count == 0 &&
            !pool->shutdown
        ) {

            pthread_cond_wait(
                &pool->not_empty,
                &pool->mutex
            );
        }


        /*
         * Shutdown when queue is empty.
         */
        if (
            pool->shutdown &&
            pool->count == 0
        ) {

            pthread_mutex_unlock(
                &pool->mutex
            );

            break;
        }


        /*
         * Get task.
         */
        Task task =
            pool->tasks[pool->front];


        pool->front =
            (pool->front + 1) % QUEUE_SIZE;


        pool->count--;


        /*
         * Tell producer that queue
         * has free space.
         */
        pthread_cond_signal(
            &pool->not_full
        );


        pthread_mutex_unlock(
            &pool->mutex
        );


        /*
         * Process client.
         */
        printf(
            "Worker %lu handling client fd=%d\n",
            (unsigned long)pthread_self(),
            task.client_fd
        );


        client_handler(
            task.client_fd,
            task.db
        );
    }


    return NULL;
}

void thread_pool_init(ThreadPool *pool) {
    pool->shutdown = 0;
    pool->front = 0;
    pool->count = 0;
    pool->rear = 0;

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->not_empty, NULL);
    pthread_cond_init(&pool->not_full, NULL);

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (pthread_create(&pool->workers[i], NULL, worker_function, pool) != 0) {
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }
    }
    printf("Thread pool created with %d workers.\n",THREAD_POOL_SIZE);
}

void thread_pool_add(ThreadPool *pool,int client_fd,HashTable *db) {
    pthread_mutex_lock(&pool->mutex);

    while (pool->count == QUEUE_SIZE && !pool->shutdown) {
        pthread_cond_wait(&pool->not_empty, &pool->mutex);
    }

    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->mutex);
        return;
    }


    pool->tasks[pool->rear].client_fd = client_fd;
    pool->tasks[pool->rear].db = db;
    pool->rear = (pool->rear + 1) % QUEUE_SIZE;
    pool->count++;

    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->mutex);
}

void thread_pool_destroy(ThreadPool *pool) {
    pthread_mutex_lock(&pool->mutex);

    pool->shutdown = 1;

    pthread_cond_broadcast(&pool->not_empty);

    pthread_mutex_unlock(&pool->mutex);

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(pool->workers[i], NULL);
    }

    pthread_mutex_destroy(&pool->mutex);

    pthread_cond_destroy(&pool->not_empty);

    pthread_cond_destroy(&pool->not_full);
}