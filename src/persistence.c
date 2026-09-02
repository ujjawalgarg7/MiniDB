#include "persistence.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>


int db_save(
    HashTable *db,
    const char *filename
)
{
    if (db == NULL ||
        filename == NULL) {

        return -1;
    }


    /*
     * Write the snapshot to a temporary file first.
     * The existing snapshot remains untouched until
     * the new file has been completely written.
     */
    char temp_filename[4096];

    int written =
        snprintf(
            temp_filename,
            sizeof(temp_filename),
            "%s.tmp",
            filename
        );


    if (written < 0 ||
        (size_t)written >= sizeof(temp_filename)) {

        fprintf(
            stderr,
            "Snapshot filename is too long.\n"
        );

        return -1;
    }


    FILE *file =
        fopen(
            temp_filename,
            "w"
        );


    if (file == NULL) {

        perror("fopen snapshot");

        return -1;
    }


    /*
     * Lock every bucket while taking
     * the snapshot.
     */
    for (unsigned int i = 0;
         i < TABLE_SIZE;
         i++) {

        pthread_rwlock_rdlock(
            &db->locks[i]
        );
    }


    int save_failed = 0;


    for (unsigned int i = 0;
         i < TABLE_SIZE;
         i++) {

        Entry *current =
            db->buckets[i];


        while (current != NULL) {

            /*
             * Don't persist an entry that is
             * already expired.
             */
            if (
                current->expires_at == 0 ||
                current->expires_at > time(NULL)
            ) {

                if (
                    fprintf(
                        file,
                        "%s\t%s\t%ld\n",
                        current->key,
                        current->value,
                        (long)current->expires_at
                    ) < 0
                ) {

                    save_failed = 1;
                    break;
                }
            }


            current =
                current->next;
        }


        if (save_failed) {
            break;
        }
    }


    /*
     * Release database locks before doing
     * filesystem synchronization.
     */
    for (unsigned int i = 0;
         i < TABLE_SIZE;
         i++) {

        pthread_rwlock_unlock(
            &db->locks[i]
        );
    }


    if (save_failed) {

        fprintf(
            stderr,
            "Failed while writing snapshot.\n"
        );

        fclose(file);
        unlink(temp_filename);

        return -1;
    }


    /*
     * Push stdio buffers into the kernel.
     */
    if (fflush(file) != 0) {

        perror("fflush snapshot");

        fclose(file);
        unlink(temp_filename);

        return -1;
    }


    /*
     * Get the underlying file descriptor so
     * the snapshot can be synchronized to disk.
     */
    int fd =
        fileno(file);


    if (fd < 0) {

        perror("fileno snapshot");

        fclose(file);
        unlink(temp_filename);

        return -1;
    }


    if (fsync(fd) != 0) {

        perror("fsync snapshot");

        fclose(file);
        unlink(temp_filename);

        return -1;
    }


    /*
     * Close the temporary snapshot before
     * atomically replacing the real snapshot.
     */
    if (fclose(file) != 0) {

        perror("fclose snapshot");

        unlink(temp_filename);

        return -1;
    }


    /*
     * rename() replaces the destination atomically
     * when source and destination are on the same
     * filesystem.
     */
    if (
        rename(
            temp_filename,
            filename
        ) != 0
    ) {

        perror("rename snapshot");

        unlink(temp_filename);

        return -1;
    }


    return 0;
}


int db_load(
    HashTable *db,
    const char *snapshot_file
)
{
    if (db == NULL ||
        snapshot_file == NULL) {

        return -1;
    }


    FILE *file =
        fopen(
            snapshot_file,
            "r"
        );


    if (file == NULL) {

        /*
         * No database file on first startup
         * is normal.
         */
        return 0;
    }


    char key[1024];
    char value[4096];
    long long expiration;


    while (
        fscanf(
            file,
            "%1023[^\t]\t%4095[^\t]\t%lld\n",
            key,
            value,
            &expiration
        ) == 3
    ) {

        time_t expires_at =
            (time_t)expiration;


        if (
            expires_at != 0 &&
            time(NULL) >= expires_at
        ) {

            continue;
        }


        db_set_expires_at(
            db,
            key,
            value,
            expires_at
        );
    }


    fclose(file);

    return 0;
}


int db_compact(
    HashTable *db,
    WAL *wal,
    const char *snapshot_file
)
{
    if (db == NULL ||
        wal == NULL ||
        snapshot_file == NULL) {

        return -1;
    }


    /*
     * Serialize persistence operations.
     */
    pthread_mutex_lock(
        &db->persistence_mutex
    );


    int result =
        db_save(
            db,
            snapshot_file
        );


    /*
     * Only reset the WAL after the snapshot
     * has been successfully written and atomically
     * installed.
     */
    if (result == 0) {

        result =
            wal_reset(wal);
    }


    pthread_mutex_unlock(
        &db->persistence_mutex
    );


    return result;
}