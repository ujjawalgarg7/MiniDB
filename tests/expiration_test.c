//
// Created by ujjawal on 22/08/26.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../src/database.h"

int main(void)
{
    printf("============================\n");
    printf("MiniDB Expiration Test\n");
    printf("============================\n\n");

    HashTable db;

    db_init(&db);

    /*
     * Set key with 3-second expiration.
     */
    printf("Setting user = Rahul for 3 seconds...\n");

    db_set_expire(
        &db,
        "user",
        "Rahul",
        3
    );

    /*
     * Immediately try GET.
     */
    char *value = db_get(
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
    }


    /*
     * Wait 4 seconds.
     */
    printf("\nWaiting 4 seconds...\n");

    sleep(4);


    /*
     * Try GET again.
     */
    value = db_get(
        &db,
        "user"
    );


    if (value == NULL) {

        printf(
            "After 4 seconds: EXPIRED\n"
        );
        printf("\nChecking again...\n");

        value = db_get(&db, "user");

        if (value == NULL) {
            printf("Second GET: key does not exist\n");
        } else {
            printf("ERROR: key still exists: %s\n", value);
            free(value);
        }

    } else {

        printf(
            "ERROR: Key still exists: %s\n",
            value
        );

        free(value);
    }


    db_destroy(&db);

    printf("\nExpiration test complete.\n");

    return 0;
}