#include <stdio.h>

#include "database.h"
#include "server.h"


int main(void)
{
    printf("MiniDB is Starting...\n");


    /*
     * Create the database.
     */
    HashTable db;

    db_init(&db);


    /*
     * Start the TCP server.
     *
     * server_start() will:
     * 1. Create the thread pool
     * 2. Create the TCP socket
     * 3. Accept clients
     * 4. Put clients into the task queue
     * 5. Workers handle the clients
     */
    if (server_start(&db) != 0) {

        fprintf(
            stderr,
            "Failed to start MiniDB server.\n"
        );

        db_destroy(&db);

        return 1;
    }


    /*
     * Cleanup.
     *
     * Currently unreachable because
     * server_start() runs continuously.
     */
    db_destroy(&db);


    return 0;
}