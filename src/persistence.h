#ifndef MINIDB_PERSISTENCE_H
#define MINIDB_PERSISTENCE_H

#include "database.h"
#include "wal.h"

int db_save(
    HashTable *db,
    const char *snapshot_file
);

int db_load(
    HashTable *db,
    const char *snapshot_file
);

int db_compact(
    HashTable *db,
    WAL *wal,
    const char *snapshot_file
);

#endif