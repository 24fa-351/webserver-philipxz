#include "http_message.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int is_complete_http_message(const char *buffer) {
    // Simple check for end of HTTP headers
    return strstr(buffer, "\r\n\r\n") != NULL;
}

void read_http_client_message(int client_socket, http_client_message_t **msg,
                              http_read_result_t *result) {
    *msg = (http_client_message_t *)malloc(sizeof(http_client_message_t));
    if (*msg == NULL) {
        *result = BAD_REQUEST;
        return;
    }

    char buffer[BUFFER_SIZE];
    buffer[0] = '\0';

    while (!is_complete_http_message(buffer)) {
        int bytes_read = read(client_socket, buffer + strlen(buffer),
                              sizeof(buffer) - strlen(buffer) - 1);
        if (bytes_read == 0) {
            *result = CLOSED_CONNECTION;
            free(*msg);
            return;
        }
        if (bytes_read < 0) {
            *result = BAD_REQUEST;
            free(*msg);
            return;
        }
        buffer[bytes_read + strlen(buffer)] = '\0';
    }

    // Parse the HTTP method
    char *method_end = strchr(buffer, ' ');
    if (method_end == NULL) {
        *result = BAD_REQUEST;
        free(*msg);
        return;
    }
    *method_end = '\0';
    (*msg)->method = strdup(buffer);
    if ((*msg)->method == NULL) {
        *result = BAD_REQUEST;
        free(*msg);
        return;
    }

    // Parse the path
    char *path_start = method_end + 1;
    char *path_end = strchr(path_start, ' ');
    if (path_end == NULL) {
        *result = BAD_REQUEST;
        free((*msg)->method);
        free(*msg);
        return;
    }
    *path_end = '\0';
    (*msg)->path = strdup(path_start);
    if ((*msg)->path == NULL) {
        *result = BAD_REQUEST;
        free((*msg)->method);
        free(*msg);
        return;
    }

    // Parse the HTTP version
    char *version_start = path_end + 1;
    char *version_end = strstr(version_start, "\r\n");
    if (version_end == NULL) {
        *result = BAD_REQUEST;
        free((*msg)->path);
        free((*msg)->method);
        free(*msg);
        return;
    }
    *version_end = '\0';
    (*msg)->http_version = strdup(version_start);
    if ((*msg)->http_version == NULL) {
        *result = BAD_REQUEST;
        free((*msg)->path);
        free((*msg)->method);
        free(*msg);
        return;
    }

    // Parse the body if present
    char *body_start = strstr(version_end + 2, "\r\n\r\n");
    if (body_start != NULL) {
        body_start += 4;
        (*msg)->body_length = strlen(body_start);
        (*msg)->body = strdup(body_start);
        if ((*msg)->body == NULL) {
            *result = BAD_REQUEST;
            free((*msg)->http_version);
            free((*msg)->path);
            free((*msg)->method);
            free(*msg);
            return;
        }
    } else {
        (*msg)->body = NULL;
        (*msg)->body_length = 0;
    }

    *result = MESSAGE;
}