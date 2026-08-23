#ifndef MINIDB_PERSISTENCE_H
#define MINIDB_PERSISTENCE_H

#include "database.h"


int db_save(
    HashTable *db,
    const char *filename
);


int db_load(
    HashTable *db,
    const char *filename
);


#endif