#ifndef WAL_H
#define WAL_H

#include <stdio.h>
#include <pthread.h>

#include "database.h"


typedef struct {

    FILE *file;

    pthread_mutex_t lock;

    char filename[512];

} WAL;


int wal_init(
    WAL *wal,
    const char *filename
);


int wal_log_set(
    WAL *wal,
    const char *key,
    const char *value,
    const char *expires_at
);


int wal_log_delete(
    WAL *wal,
    const char *key
);


int wal_replay(
    WAL *wal,
    HashTable *db
);


int wal_flush(
    WAL *wal
);


void wal_destroy(
    WAL *wal
);


int wal_reset(
    WAL *wal
);

int wal_log_flush(
    WAL *wal
);

#endif