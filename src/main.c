#include <stdio.h>

#include "database.h"
#include "server.h"
#include "wal.h"


int main(void)
{
    printf(
        "MiniDB is Starting...\n"
    );


    HashTable db;


    db_init(
        &db
    );


    WAL wal;


    if (
        wal_init(
            &wal,
            "minidb.wal"
        ) != 0
    ) {

        fprintf(
            stderr,
            "Failed to initialize WAL.\n"
        );

        db_destroy(
            &db
        );

        return 1;
    }


    /*
     * Recover previous operations.
     */
    if (
        wal_replay(
            &wal,
            &db
        ) != 0
    ) {

        fprintf(
            stderr,
            "Failed to replay WAL.\n"
        );

        wal_destroy(
            &wal
        );

        db_destroy(
            &db
        );

        return 1;
    }


    /*
     * Start server.
     */
    if (
        server_start(
            &db,
            &wal
        ) != 0
    ) {

        fprintf(
            stderr,
            "Failed to start MiniDB server.\n"
        );

        wal_destroy(
            &wal
        );

        db_destroy(
            &db
        );

        return 1;
    }


    /*
     * Normally unreachable.
     */
    wal_destroy(
        &wal
    );

    db_destroy(
        &db
    );


    return 0;
}