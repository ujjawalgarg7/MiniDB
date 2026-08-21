#include "server.h"
#include "thread_pool.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>


int server_start(HashTable *db){
    /*
     * Initialize thread pool.
     */
    ThreadPool pool;

    thread_pool_init(&pool);


    /*
     * Create TCP socket.
     */
    int server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );


    if (server_fd < 0) {

        perror("socket() failed");

        return -1;
    }


    /*
     * Allow quick reuse of the port.
     */
    int opt = 1;

    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) < 0) {

        perror("setsockopt() failed");

        close(server_fd);

        return -1;
    }


    /*
     * Configure server address.
     */
    struct sockaddr_in server_addr = {0};


    server_addr.sin_family = AF_INET;

    server_addr.sin_port =
        htons(SERVER_PORT);

    server_addr.sin_addr.s_addr =
        htonl(INADDR_ANY);


    /*
     * Bind socket.
     */
    if (bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) < 0) {

        perror("bind() failed");

        close(server_fd);

        return -1;
    }


    /*
     * Start listening.
     */
    if (listen(
            server_fd,
            10
        ) < 0) {

        perror("listen() failed");

        close(server_fd);

        return -1;
    }


    printf(
        "Server listening on port %d\n",
        SERVER_PORT
    );


    /*
     * Accept clients forever.
     */
    while (1) {

        struct sockaddr_in client_addr;

        socklen_t client_addr_len =
            sizeof(client_addr);


        int client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_addr,
            &client_addr_len
        );


        if (client_fd < 0) {

            perror("accept() failed");

            continue;
        }


        printf(
            "Client connected: fd=%d\n",
            client_fd
        );


        /*
         * Add client to thread pool.
         *
         * A worker thread will pick up
         * this client from the queue.
         */
        thread_pool_add(
            &pool,
            client_fd,
            db
        );
    }


    /*
     * This code is currently unreachable
     * because the server runs continuously.
     */
    thread_pool_destroy(&pool);

    close(server_fd);

    return 0;
}