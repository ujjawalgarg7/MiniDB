#include "../src/database.h"
#include "../src/persistence.h"
#include "../src/wal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define SNAPSHOT_FILE "compaction_test.snapshot"
#define WAL_FILE "compaction_test.wal"


int main(void)
{
    printf("============================\n");
    printf("MiniDB Compaction Test\n");
    printf("============================\n\n");


    remove(SNAPSHOT_FILE);
    remove(WAL_FILE);


    HashTable db;
    WAL wal;


    db_init(&db);


    if (wal_init(
            &wal,
            WAL_FILE
        ) != 0) {

        printf(
            "ERROR: WAL initialization failed.\n"
        );

        db_destroy(&db);

        return EXIT_FAILURE;
    }


    /*
     * Simulate database operations.
     */
    db_set(
        &db,
        "name",
        "Ujjawal"
    );

    db_set(
        &db,
        "city",
        "Noida"
    );


    /*
     * Record them in WAL.
     */
    wal_log_set(
        &wal,
        "name",
        "Ujjawal",
        "0"
    );

    wal_log_set(
        &wal,
        "city",
        "Noida",
        "0"
    );


    /*
     * Delete city.
     */
    db_delete(
        &db,
        "city"
    );

    wal_log_delete(
        &wal,
        "city"
    );


    printf(
        "Compacting database...\n"
    );


    if (
        db_compact(
            &db,
            &wal,
            SNAPSHOT_FILE
        ) != 0
    ) {

        printf(
            "ERROR: compaction failed.\n"
        );

        wal_destroy(&wal);
        db_destroy(&db);

        return EXIT_FAILURE;
    }


    printf(
        "Compaction successful.\n"
    );


    /*
     * Destroy current database.
     */
    wal_destroy(
        &wal
    );

    db_destroy(
        &db
    );


    /*
     * Create a fresh database.
     */
    HashTable loaded_db;
    WAL loaded_wal;


    db_init(
        &loaded_db
    );


    if (
        db_load(
            &loaded_db,
            SNAPSHOT_FILE
        ) != 0
    ) {

        printf(
            "ERROR: snapshot load failed.\n"
        );

        db_destroy(&loaded_db);

        return EXIT_FAILURE;
    }


    if (
        wal_init(
            &loaded_wal,
            WAL_FILE
        ) != 0
    ) {

        printf(
            "ERROR: WAL reopen failed.\n"
        );

        db_destroy(&loaded_db);

        return EXIT_FAILURE;
    }


    /*
     * WAL should now be empty,
     * but replay anyway.
     */
    wal_replay(
        &loaded_wal,
        &loaded_db
    );


    char *name =
        db_get(
            &loaded_db,
            "name"
        );


    char *city =
        db_get(
            &loaded_db,
            "city"
        );


    printf(
        "\nVerification:\n"
    );


    printf(
        "  name = %s\n",
        name != NULL
            ? name
            : "(nil)"
    );


    printf(
        "  city = %s\n",
        city != NULL
            ? city
            : "(nil)"
    );


    int passed = 1;


    if (
        name == NULL ||
        strcmp(
            name,
            "Ujjawal"
        ) != 0
    ) {

        passed = 0;
    }


    if (city != NULL) {
        passed = 0;
    }


    free(name);
    free(city);


    wal_destroy(
        &loaded_wal
    );

    db_destroy(
        &loaded_db
    );


    remove(SNAPSHOT_FILE);
    remove(WAL_FILE);


    if (passed) {

        printf(
            "\nCompaction test PASSED.\n"
        );

    } else {

        printf(
            "\nCompaction test FAILED.\n"
        );

        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}