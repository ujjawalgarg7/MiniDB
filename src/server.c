#include "server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "thread_pool.h"

#define SERVER_PORT 8080
#define BACKLOG 64
#define POLL_TIMEOUT_MS 500

static volatile sig_atomic_t server_running = 1;
static int server_fd = -1;


void server_stop(void)
{
    server_running = 0;
}


int server_start(
    HashTable *db,
    WAL *wal
)
{
    server_running = 1;

    server_fd =
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
        server_fd = -1;

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
        server_fd = -1;

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
        server_fd = -1;

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

    while (server_running) {

        struct pollfd poll_fd;

        poll_fd.fd = server_fd;
        poll_fd.events = POLLIN;
        poll_fd.revents = 0;

        int poll_result =
            poll(
                &poll_fd,
                1,
                POLL_TIMEOUT_MS
            );

        if (poll_result == 0) {
            continue;
        }

        if (poll_result < 0) {

            if (!server_running) {
                break;
            }

            perror("poll");
            continue;
        }

        if (!server_running) {
            break;
        }

        if (
            poll_fd.revents &
            (
                POLLERR |
                POLLHUP |
                POLLNVAL
            )
        ) {

            if (server_running) {
                fprintf(
                    stderr,
                    "Server socket error.\n"
                );
            }

            break;
        }

        if (!(poll_fd.revents & POLLIN)) {
            continue;
        }

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

            if (!server_running) {
                break;
            }

            perror("accept");
            continue;
        }

        if (!server_running) {
            close(client_fd);
            break;
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


    printf(
    "Stopping server...\n"
);

    if (server_fd >= 0) {

        shutdown(
            server_fd,
            SHUT_RDWR
        );

        close(
            server_fd
        );

        server_fd = -1;
    }

    thread_pool_destroy(
        &pool
    );

    printf(
        "Server stopped.\n"
    );

    return 0;
}