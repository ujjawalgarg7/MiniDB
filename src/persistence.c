#include "persistence.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


int db_save(
    HashTable *db,
    const char *filename
)
{
    if (db == NULL ||
        filename == NULL) {

        return -1;
    }


    FILE *file =
        fopen(
            filename,
            "w"
        );


    if (file == NULL) {

        perror("fopen");

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

                fprintf(
                    file,
                    "%s\t%s\t%ld\n",
                    current->key,
                    current->value,
                    (long)current->expires_at
                );
            }


            current =
                current->next;
        }
    }


    for (unsigned int i = 0;
         i < TABLE_SIZE;
         i++) {

        pthread_rwlock_unlock(
            &db->locks[i]
        );
    }


    int result =
        fclose(file);


    return result == 0 ? 0 : -1;
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

int db_compact(HashTable *db,WAL *wal,const char *snapshot_file){

    if (db == NULL || wal == NULL || snapshot_file == NULL) {
        return -1;
    }

    pthread_mutex_lock(&db->persistence_mutex);


    int result =
        db_save(db,snapshot_file);


    if (result == 0) {

        result =
            wal_reset(wal);
    }


    pthread_mutex_unlock(
        &db->persistence_mutex
    );


    return result;
}