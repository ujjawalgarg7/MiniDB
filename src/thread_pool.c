#include "thread_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>


static void send_text(
    int client_fd,
    const char *text
)
{
    send(
        client_fd,
        text,
        strlen(text),
        0
    );
}


static void client_handler(
    int client_fd,
    HashTable *db,
    WAL *wal
)
{
    char buffer[1024];


    printf(
        "Client handler started: fd=%d\n",
        client_fd
    );


    while (1) {

        ssize_t bytes_received =
            recv(
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


        buffer[bytes_received] =
            '\0';


        buffer[strcspn(
            buffer,
            "\r\n"
        )] = '\0';


        char *command =
            strtok(
                buffer,
                " "
            );


        if (command == NULL) {
            continue;
        }


        /*
         * SET
         */
        if (
            strcmp(
                command,
                "SET"
            ) == 0
        ) {

            char *key =
                strtok(NULL, " ");

            char *value =
                strtok(NULL, " ");

            char *option =
                strtok(NULL, " ");

            char *seconds_str =
                strtok(NULL, " ");


            if (
                key == NULL ||
                value == NULL
            ) {

                send_text(
                    client_fd,
                    "ERR usage: SET key value [EX seconds]\n"
                );

                continue;
            }


            /*
             * Normal SET.
             */
            if (option == NULL) {

                /*
                 * WAL first.
                 */
                if (
                    wal_log_set(
                        wal,
                        key,
                        value,
                        "0"
                    ) != 0
                ) {

                    send_text(
                        client_fd,
                        "ERR WAL write failed\n"
                    );

                    continue;
                }


                db_set(
                    db,
                    key,
                    value
                );


                send_text(
                    client_fd,
                    "OK\n"
                );

                continue;
            }


            /*
             * SET ... EX seconds
             */
            if (
                strcmp(
                    option,
                    "EX"
                ) == 0
            ) {

                if (seconds_str == NULL) {

                    send_text(
                        client_fd,
                        "ERR missing expiration time\n"
                    );

                    continue;
                }


                char *endptr = NULL;


                long seconds =
                    strtol(
                        seconds_str,
                        &endptr,
                        10
                    );


                if (
                    endptr == seconds_str ||
                    *endptr != '\0' ||
                    seconds <= 0
                ) {

                    send_text(
                        client_fd,
                        "ERR invalid expiration time\n"
                    );

                    continue;
                }


                time_t expires_at =
                    time(NULL) + seconds;


                char expiration_string[64];


                snprintf(
                    expiration_string,
                    sizeof(expiration_string),
                    "%lld",
                    (long long)expires_at
                );


                /*
                 * WAL FIRST.
                 */
                if (
                    wal_log_set(
                        wal,
                        key,
                        value,
                        expiration_string
                    ) != 0
                ) {

                    send_text(
                        client_fd,
                        "ERR WAL write failed\n"
                    );

                    continue;
                }


                db_set_expires_at(
                    db,
                    key,
                    value,
                    expires_at
                );


                send_text(
                    client_fd,
                    "OK\n"
                );

                continue;
            }


            send_text(
                client_fd,
                "ERR unknown SET option\n"
            );

            continue;
        }


        /*
         * GET
         */
        else if (
            strcmp(
                command,
                "GET"
            ) == 0
        ) {

            char *key =
                strtok(
                    NULL,
                    " "
                );


            if (key == NULL) {

                send_text(
                    client_fd,
                    "ERR usage: GET key\n"
                );

                continue;
            }


            char *value =
                db_get(
                    db,
                    key
                );


            if (value == NULL) {

                send_text(
                    client_fd,
                    "(nil)\n"
                );

            } else {

                char response[1024];


                snprintf(
                    response,
                    sizeof(response),
                    "%s\n",
                    value
                );


                send_text(
                    client_fd,
                    response
                );


                free(value);
            }


            continue;
        }


        /*
         * DEL
         */
        else if (
            strcmp(
                command,
                "DEL"
            ) == 0
        ) {

            char *key =
                strtok(
                    NULL,
                    " "
                );


            if (key == NULL) {

                send_text(
                    client_fd,
                    "ERR usage: DEL key\n"
                );

                continue;
            }


            /*
             * Log DELETE before applying it.
             */
            if (
                wal_log_delete(
                    wal,
                    key
                ) != 0
            ) {

                send_text(
                    client_fd,
                    "ERR WAL write failed\n"
                );

                continue;
            }


            int deleted =
                db_delete(
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


            send_text(
                client_fd,
                response
            );


            continue;
        }


        /*
         * EXISTS
         */
        else if (
            strcmp(
                command,
                "EXISTS"
            ) == 0
        ) {

            char *key =
                strtok(
                    NULL,
                    " "
                );


            if (key == NULL) {

                send_text(
                    client_fd,
                    "ERR usage: EXISTS key\n"
                );

                continue;
            }


            int exists =
                db_exists(
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


            send_text(
                client_fd,
                response
            );


            continue;
        }


        /*
         * EXIT
         */
        else if (
            strcmp(
                command,
                "EXIT"
            ) == 0
        ) {

            send_text(
                client_fd,
                "BYE\n"
            );

            break;
        }


        /*
         * Unknown command.
         */
        else {

            send_text(
                client_fd,
                "ERR unknown command\n"
            );
        }
    }


    close(client_fd);
}


static void *worker_function(
    void *arg
)
{
    ThreadPool *pool =
        arg;


    while (1) {

        pthread_mutex_lock(
            &pool->mutex
        );


        while (
            pool->count == 0 &&
            !pool->shutdown
        ) {

            pthread_cond_wait(
                &pool->not_empty,
                &pool->mutex
            );
        }


        if (
            pool->shutdown &&
            pool->count == 0
        ) {

            pthread_mutex_unlock(
                &pool->mutex
            );

            break;
        }


        Task task =
            pool->tasks[pool->front];


        pool->front =
            (pool->front + 1)
            % QUEUE_SIZE;


        pool->count--;


        pthread_cond_signal(
            &pool->not_full
        );


        pthread_mutex_unlock(
            &pool->mutex
        );


        printf(
            "Worker %lu handling client fd=%d\n",
            (unsigned long)pthread_self(),
            task.client_fd
        );


        client_handler(
            task.client_fd,
            task.db,
            task.wal
        );
    }


    return NULL;
}


void thread_pool_init(
    ThreadPool *pool
)
{
    pool->shutdown = 0;

    pool->front = 0;

    pool->rear = 0;

    pool->count = 0;


    pthread_mutex_init(
        &pool->mutex,
        NULL
    );

    pthread_cond_init(
        &pool->not_empty,
        NULL
    );

    pthread_cond_init(
        &pool->not_full,
        NULL
    );


    for (
        int i = 0;
        i < THREAD_POOL_SIZE;
        i++
    ) {

        if (
            pthread_create(
                &pool->workers[i],
                NULL,
                worker_function,
                pool
            ) != 0
        ) {

            perror(
                "pthread_create"
            );

            exit(EXIT_FAILURE);
        }
    }


    printf(
        "Thread pool created with %d workers.\n",
        THREAD_POOL_SIZE
    );
}


void thread_pool_add(
    ThreadPool *pool,
    int client_fd,
    HashTable *db,
    WAL *wal
)
{
    pthread_mutex_lock(
        &pool->mutex
    );


    while (
        pool->count == QUEUE_SIZE &&
        !pool->shutdown
    ) {

        pthread_cond_wait(
            &pool->not_full,
            &pool->mutex
        );
    }


    if (pool->shutdown) {

        pthread_mutex_unlock(
            &pool->mutex
        );

        return;
    }


    pool->tasks[pool->rear].client_fd =
        client_fd;

    pool->tasks[pool->rear].db =
        db;

    pool->tasks[pool->rear].wal =
        wal;


    pool->rear =
        (pool->rear + 1)
        % QUEUE_SIZE;


    pool->count++;


    pthread_cond_signal(
        &pool->not_empty
    );


    pthread_mutex_unlock(
        &pool->mutex
    );
}


void thread_pool_destroy(
    ThreadPool *pool
)
{
    pthread_mutex_lock(
        &pool->mutex
    );


    pool->shutdown = 1;


    pthread_cond_broadcast(
        &pool->not_empty
    );


    pthread_cond_broadcast(
        &pool->not_full
    );


    pthread_mutex_unlock(
        &pool->mutex
    );


    for (
        int i = 0;
        i < THREAD_POOL_SIZE;
        i++
    ) {

        pthread_join(
            pool->workers[i],
            NULL
        );
    }


    pthread_mutex_destroy(
        &pool->mutex
    );

    pthread_cond_destroy(
        &pool->not_empty
    );

    pthread_cond_destroy(
        &pool->not_full
    );
}