/**
 * @file     storage_server.h
 * @brief    Interfaccia dello storage server.
 */

#ifndef STORAGE_SERVER_H
#define STORAGE_SERVER_H

#include <protocol.h>

/**
 * @def      LOG()
 * @brief    Controlla il valore di ritorno di una chiamata di log_record()
 */
#define LOG(X) { \
    if (X == -1) { \
        fprintf(stderr, "Non è stato possibile scrivere sul file di log\n"); \
    } \
}

void open_file_handler(int master_fd, int client_fd, int worker_id, request_code_t code);

void write_file_handler(int master_fd, int client_fd, int worker_id, request_code_t code);

void read_file_handler(int master_fd, int client_fd, int worker_id);

void readn_file_handler(int master_fd, int client_fd, int worker_id);

void lock_file_handler(int master_fd, int client_fd, int worker_id);

void unlock_file_handler(int master_fd, int client_fd, int worker_id);

void remove_file_handler(int master_fd, int client_fd, int worker_id);

void close_file_handler(int master_fd, int client_fd, int worker_id);

#endif /* STORAGE_SERVER_H */