#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wal.h"


int wal_init(
    WAL *wal,
    const char *filename
)
{
    if (wal == NULL ||
        filename == NULL) {
        return -1;
    }


    memset(
        wal,
        0,
        sizeof(WAL)
    );


    strncpy(
        wal->filename,
        filename,
        sizeof(wal->filename) - 1
    );


    wal->file =
        fopen(
            wal->filename,
            "a+"
        );


    if (wal->file == NULL) {

        perror("fopen WAL");

        return -1;
    }


    if (pthread_mutex_init(
            &wal->lock,
            NULL
        ) != 0) {

        fclose(wal->file);

        wal->file = NULL;

        return -1;
    }


    return 0;
}


int wal_log_set(
    WAL *wal,
    const char *key,
    const char *value,
    const char *expires_at
)
{
    if (wal == NULL ||
        wal->file == NULL ||
        key == NULL ||
        value == NULL ||
        expires_at == NULL) {

        return -1;
    }


    pthread_mutex_lock(
        &wal->lock
    );


    int result =
        fprintf(
            wal->file,
            "SET\t%s\t%s\t%s\n",
            key,
            value,
            expires_at
        );


    int flush_result =
        fflush(wal->file);


    pthread_mutex_unlock(
        &wal->lock
    );


    if (result < 0 ||
        flush_result != 0) {

        return -1;
    }


    return 0;
}


int wal_log_delete(
    WAL *wal,
    const char *key
)
{
    if (wal == NULL ||
        wal->file == NULL ||
        key == NULL) {

        return -1;
    }


    pthread_mutex_lock(
        &wal->lock
    );


    int result =
        fprintf(
            wal->file,
            "DEL\t%s\n",
            key
        );


    int flush_result =
        fflush(wal->file);


    pthread_mutex_unlock(
        &wal->lock
    );


    if (result < 0 ||
        flush_result != 0) {

        return -1;
    }


    return 0;
}


int wal_replay(
    WAL *wal,
    HashTable *db
)
{
    if (wal == NULL ||
        wal->file == NULL ||
        db == NULL) {

        return -1;
    }


    pthread_mutex_lock(
        &wal->lock
    );


    fflush(wal->file);

    rewind(wal->file);


    char line[8192];


    while (
        fgets(
            line,
            sizeof(line),
            wal->file
        ) != NULL
    ) {

        line[strcspn(
            line,
            "\r\n"
        )] = '\0';


        /*
         * SET
         *
         * SET<TAB>key<TAB>value<TAB>expires_at
         */
        if (
            strncmp(
                line,
                "SET\t",
                4
            ) == 0
        ) {

            char *key =
                line + 4;


            char *value =
                strchr(
                    key,
                    '\t'
                );


            if (value == NULL) {
                continue;
            }


            *value = '\0';
            value++;


            char *expires_at =
                strchr(
                    value,
                    '\t'
                );


            if (expires_at == NULL) {
                continue;
            }


            *expires_at = '\0';
            expires_at++;


            char *endptr = NULL;


            long long timestamp =
                strtoll(
                    expires_at,
                    &endptr,
                    10
                );


            if (
                endptr == expires_at ||
                *endptr != '\0'
            ) {
                continue;
            }


            time_t expiration =
                (time_t)timestamp;


            /*
             * Don't restore keys that
             * expired while offline.
             */
            if (
                expiration != 0 &&
                time(NULL) >= expiration
            ) {
                continue;
            }


            db_set_expires_at(
                db,
                key,
                value,
                expiration
            );
        }

        else if (
            strcmp(
            line,
            "FLUSHDB"
            ) == 0
        ) {

            db_flush(
                db
            );
        }


        /*
         * DELETE
         *
         * DEL<TAB>key
         */
        else if (
            strncmp(
                line,
                "DEL\t",
                4
            ) == 0
        ) {

            char *key =
                line + 4;


            db_delete(
                db,
                key
            );
        }
    }


    /*
     * Future writes go to end.
     */
    fseek(
        wal->file,
        0,
        SEEK_END
    );


    pthread_mutex_unlock(
        &wal->lock
    );


    return 0;
}


int wal_flush(
    WAL *wal
)
{
    if (wal == NULL ||
        wal->file == NULL) {

        return -1;
    }


    pthread_mutex_lock(
        &wal->lock
    );


    int result =
        fflush(wal->file);


    pthread_mutex_unlock(
        &wal->lock
    );


    return result == 0 ? 0 : -1;
}


void wal_destroy(
    WAL *wal
)
{
    if (wal == NULL) {
        return;
    }


    pthread_mutex_lock(
        &wal->lock
    );


    if (wal->file != NULL) {

        fflush(wal->file);

        fclose(wal->file);

        wal->file = NULL;
    }


    pthread_mutex_unlock(
        &wal->lock
    );


    pthread_mutex_destroy(
        &wal->lock
    );
}


int wal_reset(WAL *wal)
{
    if (wal == NULL || wal->file == NULL) {
        return -1;
    }

    /*
     * Flush any buffered WAL data.
     */
    if (fflush(wal->file) != 0) {
        return -1;
    }

    /*
     * Truncate the WAL file to zero bytes.
     */
    int fd = fileno(wal->file);

    if (fd < 0) {
        return -1;
    }

    if (ftruncate(fd, 0) != 0) {
        return -1;
    }

    /*
     * Reset the FILE position to the beginning.
     */
    if (fseek(wal->file, 0, SEEK_SET) != 0) {
        return -1;
    }

    /*
     * Make sure the stream is ready for subsequent writes.
     */
    clearerr(wal->file);

    return 0;
}

int wal_log_flush(WAL *wal) {
    if (wal == NULL || wal->file == NULL) {
        return -1;
    }

    pthread_mutex_lock(&wal->lock);

    int result =
        fprintf(
            wal->file,
            "FLUSHDB\n"
        );

    int flush_result =
        fflush(wal->file);

    pthread_mutex_unlock(&wal->lock);

    if (result < 0 || flush_result != 0) {
        return -1;
    }
    return 0;
}