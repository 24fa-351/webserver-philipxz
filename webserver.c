#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http_message.h"

#define BUFFER_SIZE 1024
#define LISTEN_BACKLOG 5
#define DEFAULT_PORT 80

int request_count = 0;
int sent_bytes = 0;
int received_bytes = 0;

void handle_static_request(int client_socket, const char *file_path) {
    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "static%s", file_path);

    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        const char *response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<html><body><h1>File Not Found</h1></body></html>";
        write(client_socket, response, strlen(response));
        return;
    }

    const char *header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n\r\n";
    write(client_socket, header, strlen(header));

    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        write(client_socket, buffer, bytes_read);
    }
    close(file_fd);
}

void handle_stats_request(int client_socket) {
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
             "<html><body>"
             "<h1>Server Statistics</h1>"
             "<p>Requests received: %d</p>"
             "<p>Bytes received: %d</p>"
             "<p>Bytes sent: %d</p>"
             "</body></html>",
             request_count, received_bytes, sent_bytes);
    write(client_socket, response, strlen(response));
}

void handle_calc_request(int client_socket, const char *query) {
    int a = 0, b = 0;
    sscanf(query, "a=%d&b=%d", &a, &b);
    int sum = a + b;

    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
             "<html><body>"
             "<h1>Calculation Result</h1>"
             "<p>%d + %d = %d</p>"
             "</body></html>",
             a, b, sum);
    write(client_socket, response, strlen(response));
}

int respond_to_http_client_message(int client_socket,
                                   http_client_message_t *message) {
    request_count++;
    received_bytes += message->body_length;

    if (!strcmp(message->method, "GET")) {
        if (!strncmp(message->path, "/static", 7)) {
            handle_static_request(client_socket, message->path + 7);
        } else if (!strcmp(message->path, "/stats")) {
            handle_stats_request(client_socket);
        } else if (!strncmp(message->path, "/calc", 5)) {
            handle_calc_request(client_socket, message->path + 6);
        } else {
            const char *response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 13\r\n"
                "\r\n"
                "Hello, World!";
            write(client_socket, response, strlen(response));
        }
    } else {
        const char *response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<html><body><h1>Bad Request</h1></body></html>";
        write(client_socket, response, strlen(response));
    }
    return 0;
}

void *handleConnection(void *arg) {
    int client_socket = *((int *)arg);
    free(arg);

    http_client_message_t *http_msg = NULL;
    http_read_result_t result;

    read_http_client_message(client_socket, &http_msg, &result);
    if (result == BAD_REQUEST) {
        printf("Bad request\n");
        close(client_socket);
        return NULL;
    } else if (result == CLOSED_CONNECTION) {
        printf("Closed connection\n");
        close(client_socket);
        return NULL;
    } else if (result == MESSAGE) {
        printf(
            "Received HTTP message: method=%s, path=%s, http_version=%s, "
            "body=%s\n",
            http_msg->method, http_msg->path, http_msg->http_version,
            http_msg->body ? http_msg->body : "NULL");

        respond_to_http_client_message(client_socket, http_msg);
    }

    close(client_socket);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        port = atoi(argv[2]);
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [-p <port>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(socket_fd, (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0) {
        perror("bind");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(socket_fd, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", port);

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);
        int *client_socket = malloc(sizeof(int));
        if (!client_socket) {
            perror("Failed to allocate memory");
            continue;
        }

        *client_socket = accept(socket_fd, (struct sockaddr *)&client_address,
                                &client_address_len);
        if (*client_socket < 0) {
            perror("Failed to accept connection");
            free(client_socket);
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handleConnection, client_socket) !=
            0) {
            perror("Failed to create thread");
            close(*client_socket);
            free(client_socket);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(socket_fd);
    return 0;
}