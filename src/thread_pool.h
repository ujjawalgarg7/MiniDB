//
// Created by ujjawal on 21/08/26.
//

#ifndef MINIDB_THREAD_POOL_H
#define MINIDB_THREAD_POOL_H
#include <pthread.h>

#include "database.h"
#define THREAD_POOL_SIZE 4
#define QUEUE_SIZE 64

typedef struct {
    int client_fd;
    HashTable *db;
}Task;

typedef struct {
    Task tasks[QUEUE_SIZE];

    int front;
    int rear;
    int count;

    pthread_mutex_t mutex;

    pthread_cond_t not_empty;
    pthread_cond_t not_full;

    pthread_t workers[THREAD_POOL_SIZE];

    int shutdown;
} ThreadPool;

void thread_pool_init(ThreadPool *pool);
void thread_pool_add(ThreadPool *pool, int client_fd,HashTable *db);
void thread_pool_destroy(ThreadPool *pool);

#endif //MINIDB_THREAD_POOL_H
