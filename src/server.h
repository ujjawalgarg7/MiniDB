#ifndef MINIDB_SERVER_H
#define MINIDB_SERVER_H

#include "database.h"
#include "wal.h"


int server_start(
    HashTable *db,
    WAL *wal
);

void server_stop(void);
#endif