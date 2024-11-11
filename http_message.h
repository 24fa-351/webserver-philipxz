#ifndef HTTP_MESSAGE_H
#define HTTP_MESSAGE_H

#define BUFFER_SIZE 1024

typedef struct {
    char *method;
    char *path;
    char *http_version;
    char *body;
    int body_length;
} http_client_message_t;

typedef enum { MESSAGE, BAD_REQUEST, CLOSED_CONNECTION } http_read_result_t;

int is_complete_http_message(const char *buffer);
void read_http_client_message(int client_socket, http_client_message_t **msg,
                              http_read_result_t *result);

#endif  // HTTP_MESSAGE_H