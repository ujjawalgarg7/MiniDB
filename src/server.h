//
// Created by ujjawal on 19/08/26.
//

#ifndef MINIDB_SERVER_H
#define MINIDB_SERVER_H

#include"database.h"

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

int server_start(HashTable *db);

#endif //MINIDB_SERVER_H
