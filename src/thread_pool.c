#include "thread_pool.h"
#include "persistence.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>


#define SNAPSHOT_FILE "minidb.snapshot"

#define CLIENT_BUFFER_SIZE 8192


static void send_text(
    int client_fd,
    const char *text
)
{
    if (text == NULL) {
        return;
    }

    send(
        client_fd,
        text,
        strlen(text),
        0
    );
}


static void uppercase_string(
    char *str
)
{
    if (str == NULL) {
        return;
    }

    while (*str != '\0') {

        *str =
            (char)toupper(
                (unsigned char)*str
            );

        str++;
    }
}


/*
 * Remove leading and trailing spaces.
 */
static char *trim_spaces(
    char *str
)
{
    if (str == NULL) {
        return NULL;
    }


    while (*str == ' ') {
        str++;
    }


    char *end =
        str + strlen(str);


    while (
        end > str &&
        end[-1] == ' '
    ) {

        end--;

        *end = '\0';
    }


    return str;
}


/*
 * Process exactly ONE complete command line.
 *
 * Returns:
 *
 *   0  -> continue connection
 *  -1  -> close connection
 */
static int process_command(
    int client_fd,
    char *buffer,
    HashTable *db,
    WAL *wal
)
{
    char *saveptr = NULL;
    if (
        buffer == NULL ||
        db == NULL ||
        wal == NULL
    ) {

        return -1;
    }


    /*
     * Remove CR/LF.
     *
     * Normally the caller already removed '\n',
     * but this also handles '\r'.
     */
    buffer[
        strcspn(
            buffer,
            "\r\n"
        )
    ] = '\0';


    char *line =
        trim_spaces(buffer);


    if (
        line == NULL ||
        *line == '\0'
    ) {

        return 0;
    }


    /*
     * Extract command.
     *
     * strtok_r() modifies only the command portion.
     */
    char *command =
        strtok_r(
            line,
            " ",
            &saveptr
        );


    if (command == NULL) {
        return 0;
    }


    /*
     * Commands are case-insensitive.
     */
    uppercase_string(
        command
    );


    /*
     * ========================================================
     * PING
     * ========================================================
     */
    if (
        strcmp(
            command,
            "PING"
        ) == 0
    ) {

        send_text(
            client_fd,
            "PONG\n"
        );

        return 0;
    }


    /*
     * ========================================================
     * SET
     * ========================================================
     *
     * Supported:
     *
     * SET key value
     * SET key value with spaces
     * SET key value EX seconds
     * SET key value with spaces EX seconds
     */
    if (
        strcmp(
            command,
            "SET"
        ) == 0
    ) {

        /*
         * Get key.
         */
        char *key =
            strtok_r(
                NULL,
                " ",
                &saveptr
            );


        /*
         * Get EVERYTHING after the key.
         *
         * Example:
         *
         * SET message hello world
         *
         * value becomes:
         *
         * hello world
         */
        char *value =
            strtok_r(
                NULL,
                "",
                &saveptr
            );


        if (
            key == NULL ||
            value == NULL
        ) {

            send_text(
                client_fd,
                "ERR usage: SET key value [EX seconds]\n"
            );

            return 0;
        }


        /*
         * Remove leading spaces.
         */
        while (*value == ' ') {
            value++;
        }


        /*
         * Find EX.
         *
         * We only treat EX as an expiration option
         * when it appears as a separate token.
         */
        char *ex_position = NULL;


        char *scan =
            value;


        while (*scan != '\0') {

            if (
                (scan[0] == 'E' ||
                 scan[0] == 'e') &&

                (scan[1] == 'X' ||
                 scan[1] == 'x') &&

                scan[2] == ' ' &&

                (
                    scan == value ||
                    scan[-1] == ' '
                )
            ) {

                /*
                 * EX must be separated from the
                 * value by a space on the left too,
                 * unless it starts the value.
                 */
                if (
                    scan == value ||
                    scan[-1] == ' '
                ) {

                    ex_position =
                        scan;

                    break;
                }
            }


            scan++;
        }


        /*
         * ----------------------------------------------------
         * NORMAL SET
         * ----------------------------------------------------
         */
        if (ex_position == NULL) {

            value =
                trim_spaces(value);


            if (
                value == NULL ||
                *value == '\0'
            ) {

                send_text(
                    client_fd,
                    "ERR usage: SET key value [EX seconds]\n"
                );

                return 0;
            }


            pthread_mutex_lock(
                &db->persistence_mutex
            );


            /*
             * WAL first.
             */
            int wal_result =
                wal_log_set(
                    wal,
                    key,
                    value,
                    "0"
                );


            /*
             * Only update the database if
             * the WAL write succeeded.
             */
            if (wal_result == 0) {

                db_set(
                    db,
                    key,
                    value
                );
            }


            pthread_mutex_unlock(
                &db->persistence_mutex
            );


            if (wal_result != 0) {

                send_text(
                    client_fd,
                    "ERR WAL write failed\n"
                );

                return 0;
            }


            send_text(
                client_fd,
                "OK\n"
            );


            return 0;
        }


        /*
         * ----------------------------------------------------
         * SET ... EX seconds
         * ----------------------------------------------------
         */


        /*
         * Cut off the value before EX.
         */
        *ex_position =
            '\0';


        value =
            trim_spaces(value);


        if (
            value == NULL ||
            *value == '\0'
        ) {

            send_text(
                client_fd,
                "ERR usage: SET key value [EX seconds]\n"
            );

            return 0;
        }


        /*
         * ex_position currently points to:
         *
         * EX 10
         *
         * so +2 gets us past EX,
         * and then we skip spaces.
         */
        char *seconds_str =
            ex_position + 2;


        while (*seconds_str == ' ') {
            seconds_str++;
        }


        /*
         * Missing expiration.
         */
        if (*seconds_str == '\0') {

            send_text(
                client_fd,
                "ERR missing expiration time\n"
            );

            return 0;
        }


        /*
         * Validate the number.
         */
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

            return 0;
        }


        /*
         * Calculate absolute expiration timestamp.
         */
        time_t expires_at =
            time(NULL) + seconds;


        char expiration_string[64];


        snprintf(
            expiration_string,
            sizeof(expiration_string),
            "%lld",
            (long long)expires_at
        );


        pthread_mutex_lock(
            &db->persistence_mutex
        );


        /*
         * WAL first.
         */
        int wal_result =
            wal_log_set(
                wal,
                key,
                value,
                expiration_string
            );


        /*
         * Only update database if WAL succeeded.
         */
        if (wal_result == 0) {

            db_set_expires_at(
                db,
                key,
                value,
                expires_at
            );
        }


        pthread_mutex_unlock(
            &db->persistence_mutex
        );


        if (wal_result != 0) {

            send_text(
                client_fd,
                "ERR WAL write failed\n"
            );

            return 0;
        }


        send_text(
            client_fd,
            "OK\n"
        );


        return 0;
    }


    /*
     * ========================================================
     * GET
     * ========================================================
     */
    if (
        strcmp(
            command,
            "GET"
        ) == 0
    ) {

        char *key =
            strtok_r(
                NULL,
                " ",
                &saveptr
            );


        if (key == NULL) {

            send_text(
                client_fd,
                "ERR usage: GET key\n"
            );

            return 0;
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


        return 0;
    }


    /*
     * ========================================================
     * DEL
     * ========================================================
     */
    if (
        strcmp(
            command,
            "DEL"
        ) == 0
    ) {

        char *key =
            strtok_r(
                NULL,
                " ",
                &saveptr
            );


        if (key == NULL) {

            send_text(
                client_fd,
                "ERR usage: DEL key\n"
            );

            return 0;
        }


        pthread_mutex_lock(
            &db->persistence_mutex
        );


        /*
         * WAL first.
         */
        int wal_result =
            wal_log_delete(
                wal,
                key
            );


        int deleted = 0;


        if (wal_result == 0) {

            deleted =
                db_delete(
                    db,
                    key
                );
        }


        pthread_mutex_unlock(
            &db->persistence_mutex
        );


        if (wal_result != 0) {

            send_text(
                client_fd,
                "ERR WAL write failed\n"
            );

            return 0;
        }


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


        return 0;
    }


    /*
     * ========================================================
     * EXISTS
     * ========================================================
     */
    if (
        strcmp(
            command,
            "EXISTS"
        ) == 0
    ) {

        char *key =
            strtok_r(
                NULL,
                " ",
                &saveptr
            );


        if (key == NULL) {

            send_text(
                client_fd,
                "ERR usage: EXISTS key\n"
            );

            return 0;
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


        return 0;
    }


    /*
     * ========================================================
     * SAVE
     * ========================================================
     */
    if (
        strcmp(
            command,
            "SAVE"
        ) == 0
    ) {

        /*
         * db_compact() owns the persistence mutex.
         *
         * Do not lock it again here.
         */
        int result =
            db_compact(
                db,
                wal,
                SNAPSHOT_FILE
            );


        if (result != 0) {

            send_text(
                client_fd,
                "ERR save failed\n"
            );

            return 0;
        }


        send_text(
            client_fd,
            "OK\n"
        );


        return 0;
    }


    /*
     * ========================================================
     * INFO
     * ========================================================
     */
    if (
        strcmp(
            command,
            "INFO"
        ) == 0
    ) {

        int key_count =
            db_count_keys(
                db
            );


        if (key_count < 0) {

            send_text(
                client_fd,
                "ERR info failed\n"
            );

            return 0;
        }


        char response[256];


        snprintf(
            response,
            sizeof(response),
            "MiniDB\nkeys: %d\nbuckets: %d\n",
            key_count,
            TABLE_SIZE
        );


        send_text(
            client_fd,
            response
        );


        return 0;
    }


    /*
     * ========================================================
     * FLUSHDB
     * ========================================================
     */
    if (
        strcmp(
            command,
            "FLUSHDB"
        ) == 0
    ) {

        pthread_mutex_lock(
            &db->persistence_mutex
        );


        int wal_result =
            wal_log_flush(
                wal
            );


        int result = -1;


        if (wal_result == 0) {

            result =
                db_flush(
                    db
                );
        }


        pthread_mutex_unlock(
            &db->persistence_mutex
        );


        if (wal_result != 0) {

            send_text(
                client_fd,
                "ERR WAL write failed\n"
            );

            return 0;
        }


        if (result != 0) {

            send_text(
                client_fd,
                "ERR flush failed\n"
            );

            return 0;
        }


        send_text(
            client_fd,
            "OK\n"
        );


        return 0;
    }


    /*
     * ========================================================
     * HELP
     * ========================================================
     */
    if (
        strcmp(
            command,
            "HELP"
        ) == 0
    ) {

        send_text(
            client_fd,
            "Commands:\n"
            "SET key value [EX seconds]\n"
            "GET key\n"
            "DEL key\n"
            "EXISTS key\n"
            "SAVE\n"
            "FLUSHDB\n"
            "INFO\n"
            "PING\n"
            "HELP\n"
            "EXIT\n"
        );


        return 0;
    }


    /*
     * ========================================================
     * EXIT
     * ========================================================
     */
    if (
        strcmp(
            command,
            "EXIT"
        ) == 0
    ) {

        send_text(
            client_fd,
            "BYE\n"
        );

        return -1;
    }


    /*
     * Unknown command.
     */
    send_text(
        client_fd,
        "ERR unknown command\n"
    );


    return 0;
}


/*
 * ============================================================
 * Client handler
 * ============================================================
 *
 * This is the important TCP framing fix.
 *
 * A single recv() can contain:
 *
 *   SET a 1\nSET b 2\nSET c 3\n
 *
 * or:
 *
 *   SET a
 *
 * followed by another recv():
 *
 *   1\n
 *
 * We therefore keep data in an input buffer and process
 * complete newline-terminated commands one at a time.
 */
static void client_handler(
    int client_fd,
    HashTable *db,
    WAL *wal,
    ThreadPool *pool
)
{
    struct timeval timeout;


    timeout.tv_sec =
        1;

    timeout.tv_usec =
        0;


    setsockopt(
        client_fd,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &timeout,
        sizeof(timeout)
    );


    char input_buffer[
        CLIENT_BUFFER_SIZE
    ];


    size_t buffer_used = 0;


    printf(
        "Client handler started: fd=%d\n",
        client_fd
    );


    while (1) {

        /*
         * Check shutdown state.
         */
        pthread_mutex_lock(
            &pool->mutex
        );


        int shutting_down =
            pool->shutdown;


        pthread_mutex_unlock(
            &pool->mutex
        );


        if (shutting_down) {

            printf(
                "Closing client fd=%d during shutdown.\n",
                client_fd
            );

            break;
        }


        /*
         * Make sure there is always space for
         * a terminating '\0'.
         */
        if (
            buffer_used >=
            sizeof(input_buffer) - 1
        ) {

            send_text(
                client_fd,
                "ERR command too long\n"
            );

            buffer_used = 0;

            continue;
        }


        /*
         * Receive more TCP data.
         */
        ssize_t bytes_received =
            recv(
                client_fd,
                input_buffer + buffer_used,
                sizeof(input_buffer) -
                    buffer_used -
                    1,
                0
            );


        if (bytes_received < 0) {

            if (
                errno == EAGAIN ||
                errno == EWOULDBLOCK
            ) {

                /*
                 * Timeout gives us another chance
                 * to notice shutdown.
                 */
                continue;
            }


            if (errno == EINTR) {
                continue;
            }


            perror("recv");

            break;
        }


        /*
         * Client closed the connection.
         */
        if (bytes_received == 0) {

            /*
             * Process a final unterminated command,
             * if one exists.
             *
             * This makes the server a little more
             * forgiving of clients that close without
             * sending a final newline.
             */
            if (buffer_used > 0) {

                input_buffer[
                    buffer_used
                ] = '\0';


                int result =
                    process_command(
                        client_fd,
                        input_buffer,
                        db,
                        wal
                    );


                if (result < 0) {
                    break;
                }
            }


            printf(
                "Client disconnected: fd=%d\n",
                client_fd
            );

            break;
        }


        buffer_used +=
            (size_t)bytes_received;


        input_buffer[
            buffer_used
        ] = '\0';


        /*
         * Process EVERY complete command currently
         * sitting in the receive buffer.
         */
        while (1) {

            char *newline =
                strchr(
                    input_buffer,
                    '\n'
                );


            if (newline == NULL) {
                break;
            }


            /*
             * Temporarily terminate this command.
             */
            *newline = '\0';


            int result =
                process_command(
                    client_fd,
                    input_buffer,
                    db,
                    wal
                );


            /*
             * How many bytes remain after
             * this command?
             */
            size_t consumed =
                (size_t)(
                    newline -
                    input_buffer
                ) + 1;


            size_t remaining =
                buffer_used -
                consumed;


            /*
             * Move remaining commands to
             * the beginning of the buffer.
             */
            if (remaining > 0) {

                memmove(
                    input_buffer,
                    input_buffer + consumed,
                    remaining
                );
            }


            buffer_used =
                remaining;


            input_buffer[
                buffer_used
            ] = '\0';


            /*
             * EXIT closes this client.
             */
            if (result < 0) {

                close(client_fd);

                return;
            }


            /*
             * There may be another complete command
             * already in the buffer.
             *
             * Go around again.
             */
        }
    }


    close(
        client_fd
    );
}


/*
 * ============================================================
 * Worker thread
 * ============================================================
 */
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


        /*
         * Shutdown and no queued work.
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


        Task task =
            pool->tasks[
                pool->front
            ];


        pool->front =
            (
                pool->front + 1
            ) % QUEUE_SIZE;


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
            task.wal,
            pool
        );
    }


    return NULL;
}


/*
 * ============================================================
 * Initialize thread pool
 * ============================================================
 */
void thread_pool_init(
    ThreadPool *pool
)
{
    pool->shutdown =
        0;

    pool->front =
        0;

    pool->rear =
        0;

    pool->count =
        0;


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


/*
 * ============================================================
 * Add client to queue
 * ============================================================
 */
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


    /*
     * Pool is shutting down.
     */
    if (pool->shutdown) {

        pthread_mutex_unlock(
            &pool->mutex
        );


        close(
            client_fd
        );


        return;
    }


    pool->tasks[
        pool->rear
    ].client_fd =
        client_fd;


    pool->tasks[
        pool->rear
    ].db =
        db;


    pool->tasks[
        pool->rear
    ].wal =
        wal;


    pool->rear =
        (
            pool->rear + 1
        ) % QUEUE_SIZE;


    pool->count++;


    pthread_cond_signal(
        &pool->not_empty
    );


    pthread_mutex_unlock(
        &pool->mutex
    );
}


/*
 * ============================================================
 * Destroy thread pool
 * ============================================================
 */
void thread_pool_destroy(
    ThreadPool *pool
)
{
    pthread_mutex_lock(
        &pool->mutex
    );


    pool->shutdown =
        1;


    /*
     * Wake all workers.
     */
    pthread_cond_broadcast(
        &pool->not_empty
    );


    pthread_cond_broadcast(
        &pool->not_full
    );


    pthread_mutex_unlock(
        &pool->mutex
    );


    /*
     * Active client handlers check shutdown
     * at least once per second because of
     * SO_RCVTIMEO.
     */
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