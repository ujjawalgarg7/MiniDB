//
// Created by ujjawal on 21/08/26.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

#define NUM_CLIENTS 10
#define OPERATIONS_PER_CLIENT 100


typedef struct {
    int client_id;
} ClientArgs;


/*
 * Connect to MiniDB.
 */
int connect_to_server(void)
{
    int socket_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (socket_fd < 0) {
        perror("socket");
        return -1;
    }


    struct sockaddr_in server_addr;

    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );


    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);


    if (inet_pton(
            AF_INET,
            SERVER_IP,
            &server_addr.sin_addr
        ) <= 0) {

        perror("inet_pton");

        close(socket_fd);

        return -1;
    }


    if (connect(
            socket_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) < 0) {

        perror("connect");

        close(socket_fd);

        return -1;
    }


    return socket_fd;
}


/*
 * Send command and read response.
 */
int send_command(
    int socket_fd,
    const char *command
)
{
    char buffer[1024];


    send(
        socket_fd,
        command,
        strlen(command),
        0
    );


    ssize_t bytes = recv(
        socket_fd,
        buffer,
        sizeof(buffer) - 1,
        0
    );


    if (bytes <= 0) {
        return -1;
    }


    buffer[bytes] = '\0';


    return 0;
}


/*
 * One client thread.
 */
void *client_worker(void *arg)
{
    ClientArgs *args = (ClientArgs *)arg;

    int client_id = args->client_id;


    int socket_fd = connect_to_server();


    if (socket_fd < 0) {
        return NULL;
    }


    printf(
        "Client %d connected\n",
        client_id
    );


    for (int i = 0;
         i < OPERATIONS_PER_CLIENT;
         i++) {

        char command[256];


        /*
         * SET
         */

        snprintf(
            command,
            sizeof(command),
            "SET client%d_key%d value%d\n",
            client_id,
            i,
            i
        );


        if (send_command(
                socket_fd,
                command
            ) < 0) {

            printf(
                "Client %d: SET failed\n",
                client_id
            );

            break;
        }


        /*
         * GET
         */

        snprintf(
            command,
            sizeof(command),
            "GET client%d_key%d\n",
            client_id,
            i
        );


        if (send_command(
                socket_fd,
                command
            ) < 0) {

            printf(
                "Client %d: GET failed\n",
                client_id
            );

            break;
        }
    }


    /*
     * Close connection.
     */

    send_command(
        socket_fd,
        "EXIT\n"
    );


    close(socket_fd);


    printf(
        "Client %d finished\n",
        client_id
    );


    return NULL;
}


int main(void)
{
    printf(
        "MiniDB stress test starting...\n"
    );


    pthread_t threads[NUM_CLIENTS];

    ClientArgs args[NUM_CLIENTS];


    /*
     * Create clients.
     */

    for (int i = 0;
         i < NUM_CLIENTS;
         i++) {

        args[i].client_id = i;


        if (pthread_create(
                &threads[i],
                NULL,
                client_worker,
                &args[i]
            ) != 0) {

            perror("pthread_create");

            return EXIT_FAILURE;
        }
    }


    /*
     * Wait for clients.
     */

    for (int i = 0;
         i < NUM_CLIENTS;
         i++) {

        pthread_join(
            threads[i],
            NULL
        );
    }


    printf(
        "\nStress test completed.\n"
    );


    return 0;
}