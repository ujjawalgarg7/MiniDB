#include "server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "thread_pool.h"


#define SERVER_PORT 8080
#define BACKLOG 64


int server_start(
    HashTable *db,
    WAL *wal
)
{
    int server_fd =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );


    if (server_fd < 0) {

        perror("socket");

        return -1;
    }


    int option = 1;


    if (
        setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &option,
            sizeof(option)
        ) < 0
    ) {

        perror("setsockopt");

        close(server_fd);

        return -1;
    }


    struct sockaddr_in server_addr;


    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );


    server_addr.sin_family =
        AF_INET;

    server_addr.sin_addr.s_addr =
        htonl(INADDR_ANY);

    server_addr.sin_port =
        htons(SERVER_PORT);


    if (
        bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) < 0
    ) {

        perror("bind");

        close(server_fd);

        return -1;
    }


    if (
        listen(
            server_fd,
            BACKLOG
        ) < 0
    ) {

        perror("listen");

        close(server_fd);

        return -1;
    }


    ThreadPool pool;


    thread_pool_init(
        &pool
    );


    printf(
        "Server listening on port %d\n",
        SERVER_PORT
    );


    while (1) {

        struct sockaddr_in client_addr;

        socklen_t client_len =
            sizeof(client_addr);


        int client_fd =
            accept(
                server_fd,
                (struct sockaddr *)&client_addr,
                &client_len
            );


        if (client_fd < 0) {

            perror("accept");

            continue;
        }


        printf(
            "Client connected: fd=%d\n",
            client_fd
        );


        thread_pool_add(
            &pool,
            client_fd,
            db,
            wal
        );
    }


    /*
     * Normally unreachable.
     */
    thread_pool_destroy(
        &pool
    );


    close(server_fd);


    return 0;
}