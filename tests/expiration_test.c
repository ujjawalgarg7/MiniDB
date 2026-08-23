#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "../src/database.h"


int main(void)
{
    printf("============================\n");
    printf("MiniDB Expiration Test\n");
    printf("============================\n\n");


    HashTable db;

    db_init(&db);


    printf(
        "Setting user = Rahul for 3 seconds...\n"
    );


    db_set_expires_at(
        &db,
        "user",
        "Rahul",
        time(NULL) + 3
    );


    char *value =
        db_get(
            &db,
            "user"
        );


    if (value != NULL) {

        printf(
            "Immediately: %s\n",
            value
        );

        free(value);

    } else {

        printf(
            "ERROR: Key expired too early!\n"
        );

        db_destroy(&db);

        return EXIT_FAILURE;
    }


    printf(
        "\nWaiting 4 seconds...\n"
    );


    sleep(4);


    value =
        db_get(
            &db,
            "user"
        );


    if (value == NULL) {

        printf(
            "After 4 seconds: EXPIRED\n"
        );

    } else {

        printf(
            "ERROR: Key still exists: %s\n",
            value
        );

        free(value);

        db_destroy(&db);

        return EXIT_FAILURE;
    }


    printf(
        "\nChecking again...\n"
    );


    value =
        db_get(
            &db,
            "user"
        );


    if (value == NULL) {

        printf(
            "Second GET: key does not exist\n"
        );

    } else {

        printf(
            "ERROR: Second GET returned: %s\n",
            value
        );

        free(value);

        db_destroy(&db);

        return EXIT_FAILURE;
    }


    db_destroy(&db);


    printf(
        "\nExpiration test complete.\n"
    );


    return EXIT_SUCCESS;
}