#include <stdio.h>
#include <stdlib.h>
#include "database.h"

int main(void) {

    printf("MiniDB is Starting...!\n");
    HashTable db;

    db_init(&db);


    db_set(&db, "name","Ujjawal");

    db_set(&db, "city","Noida");

    char *name = db_get(&db, "name");
    char *city = db_get(&db, "city");

    if (name != NULL) {
        printf("Name = %s\n",name);
        free(name);
    }
    if (city != NULL) {
        printf("City = %s\n",city);
        free(city);
    }

    printf("name exists = %d\n",db_exists(&db, "name"));

    printf("age exists = %d\n",db_exists(&db, "age"));

    printf("delete city = %d\n",db_delete(&db, "city"));

    printf("city exists = %d\n",db_exists(&db, "city"));

    db_destroy(&db);


    return 0;
}
