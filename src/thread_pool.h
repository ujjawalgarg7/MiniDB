#ifndef MINIDB_THREAD_POOL_H
#define MINIDB_THREAD_POOL_H

#include <pthread.h>

#include "database.h"
#include "wal.h"


#define THREAD_POOL_SIZE 4
#define QUEUE_SIZE 64


typedef struct {

    int client_fd;

    HashTable *db;

    WAL *wal;

} Task;


typedef struct {

    Task tasks[QUEUE_SIZE];

    int front;

    int rear;

    int count;

    int shutdown;

    pthread_mutex_t mutex;

    pthread_cond_t not_empty;

    pthread_cond_t not_full;

    pthread_t workers[THREAD_POOL_SIZE];

} ThreadPool;


void thread_pool_init(
    ThreadPool *pool
);


void thread_pool_add(
    ThreadPool *pool,
    int client_fd,
    HashTable *db,
    WAL *wal
);


void thread_pool_destroy(
    ThreadPool *pool
);


#endif