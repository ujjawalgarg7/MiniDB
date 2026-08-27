#include <signal.h>
#include <stdio.h>

#include "database.h"
#include "server.h"
#include "wal.h"
#include "persistence.h"


#define SNAPSHOT_FILE "minidb.snapshot"
#define WAL_FILE      "minidb.wal"


static void handle_signal(
    int signal_number
)
{
    (void)signal_number;

    server_stop();
}


int main(void)
{
    printf(
        "MiniDB is Starting...\n"
    );


    /*
     * Handle Ctrl+C.
     */
    signal(
        SIGINT,
        handle_signal
    );

    signal(
        SIGTERM,
        handle_signal
    );


    /*
     * Create database.
     */
    HashTable db;

    db_init(
        &db
    );


    /*
     * Initialize WAL.
     */
    WAL wal;

    if (
        wal_init(
            &wal,
            WAL_FILE
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
     * --------------------------------------------------------
     * RECOVERY
     * --------------------------------------------------------
     *
     * First load the latest snapshot.
     */
    printf(
        "Loading snapshot...\n"
    );

    if (
        db_load(
            &db,
            SNAPSHOT_FILE
        ) != 0
    ) {

        fprintf(
            stderr,
            "Failed to load snapshot.\n"
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
     * Then replay WAL records that happened
     * after the snapshot.
     */
    printf(
        "Replaying WAL...\n"
    );

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


    printf(
        "Recovery complete.\n"
    );


    /*
     * Start TCP server.
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
     * Clean shutdown.
     */
    printf(
        "Closing WAL...\n"
    );

    wal_destroy(
        &wal
    );


    printf(
        "Destroying database...\n"
    );

    db_destroy(
        &db
    );


    printf(
        "MiniDB shutdown complete.\n"
    );


    return 0;
}