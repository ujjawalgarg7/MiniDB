#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/database.h"
#include "../src/wal.h"


int main(void)
{
    printf("============================\n");
    printf("MiniDB WAL Test\n");
    printf("============================\n\n");


    HashTable db;

    WAL wal;


    db_init(
        &db
    );


    if (
        wal_init(
            &wal,
            "minidb.wal"
        ) != 0
    ) {

        printf(
            "ERROR: WAL initialization failed.\n"
        );

        db_destroy(
            &db
        );

        return EXIT_FAILURE;
    }


    /*
     * Make sure this test starts clean.
     */
    freopen(
        "minidb.wal",
        "w+",
        wal.file
    );


    printf(
        "Writing WAL records...\n"
    );


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


    wal_log_set(
        &wal,
        "language",
        "C",
        "0"
    );


    wal_flush(
        &wal
    );


    printf(
        "WAL written successfully.\n\n"
    );


    db_destroy(
        &db
    );


    db_init(
        &db
    );


    printf(
        "Replaying WAL...\n"
    );


    if (
        wal_replay(
            &wal,
            &db
        ) != 0
    ) {

        printf(
            "ERROR: WAL replay failed.\n"
        );

        wal_destroy(
            &wal
        );

        db_destroy(
            &db
        );

        return EXIT_FAILURE;
    }


    char *name =
        db_get(
            &db,
            "name"
        );


    char *city =
        db_get(
            &db,
            "city"
        );


    char *language =
        db_get(
            &db,
            "language"
        );


    printf(
        "\nVerification:\n"
    );


    printf(
        "  name     = %s\n",
        name ? name : "(nil)"
    );


    printf(
        "  city     = %s\n",
        city ? city : "(nil)"
    );


    printf(
        "  language = %s\n",
        language ? language : "(nil)"
    );


    int passed = 1;


    if (
        name == NULL ||
        strcmp(name, "Ujjawal") != 0
    ) {
        passed = 0;
    }


    if (
        city == NULL ||
        strcmp(city, "Noida") != 0
    ) {
        passed = 0;
    }


    if (
        language == NULL ||
        strcmp(language, "C") != 0
    ) {
        passed = 0;
    }


    free(name);
    free(city);
    free(language);


    if (passed) {

        printf(
            "\nWAL test PASSED.\n"
        );

    } else {

        printf(
            "\nWAL test FAILED.\n"
        );
    }


    wal_destroy(
        &wal
    );


    db_destroy(
        &db
    );


    remove(
        "minidb.wal"
    );


    printf(
        "\nWAL test complete.\n"
    );


    return passed
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}