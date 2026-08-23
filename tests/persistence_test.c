#include "../src/database.h"
#include "../src/persistence.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define TEST_FILE "persistence_test.db"


int main(void)
{
    printf("============================\n");
    printf("MiniDB Persistence Test\n");
    printf("============================\n\n");


    HashTable db;


    db_init(
        &db
    );


    printf(
        "Setting values...\n"
    );


    db_set_expires_at(
        &db,
        "name",
        "Ujjawal",
        0
    );


    db_set_expires_at(
        &db,
        "city",
        "Noida",
        0
    );


    db_set_expires_at(
        &db,
        "language",
        "C",
        0
    );


    printf(
        "Saving database...\n"
    );


    if (
        db_save(
            &db,
            TEST_FILE
        ) != 0
    ) {

        printf(
            "ERROR: db_save() failed\n"
        );

        db_destroy(
            &db
        );

        return EXIT_FAILURE;
    }


    printf(
        "Database saved successfully.\n"
    );


    db_destroy(
        &db
    );


    printf(
        "Original database destroyed.\n\n"
    );


    HashTable loaded_db;


    db_init(
        &loaded_db
    );


    printf(
        "Loading database...\n"
    );


    if (
        db_load(
            &loaded_db,
            TEST_FILE
        ) != 0
    ) {

        printf(
            "ERROR: db_load() failed\n"
        );

        db_destroy(
            &loaded_db
        );

        return EXIT_FAILURE;
    }


    printf(
        "Database loaded successfully.\n\n"
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


    char *language =
        db_get(
            &loaded_db,
            "language"
        );


    printf(
        "Verification:\n"
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


    db_destroy(
        &loaded_db
    );


    remove(
        TEST_FILE
    );


    if (passed) {

        printf(
            "\nPersistence test PASSED.\n"
        );

    } else {

        printf(
            "\nPersistence test FAILED.\n"
        );

        return EXIT_FAILURE;
    }


    printf(
        "\nPersistence test complete.\n"
    );


    return EXIT_SUCCESS;
}