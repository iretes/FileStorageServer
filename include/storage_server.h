/**
 * @file     storage_server.h
 * @brief    Interfaccia dello storage server.
 */

#ifndef STORAGE_SERVER_H
#define STORAGE_SERVER_H

#include <protocol.h>
#include <config_parser.h>
#include <logger.h>

/**
 * @def      LOG()
 * @brief    Controlla il valore di ritorno di una chiamata di log_record()
 */
#define LOG(X) { \
    if (X == -1) { \
        fprintf(stderr, "Non è stato possibile scrivere sul file di log\n"); \
    } \
}

/* Struttura che rappresenta lo storage */
typedef struct storage storage_t;

/**
 * @function        create_storage()
 * @brief           Inizializza la struttura che rappresenta lo storage e ritorna un puntatore ad essa. 
 *                  Inizializza i campi con i valori iniziali o con i valori dei parametri di configurazione.
 * 
 * @param config    Parametri di configurazione
 * 
 * @return          Un puntatore a una struttura che rappresenta lo storage in caso di successo, 
 *                  NULL in caso di fallimento con errno settato a indicare l'errore.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se config è @c NULL o config->max_file_num <= 0 o config->max_bytes <= 0 o 
 *                         config->max_locks <= 0 o config->expected_clients <= 0
 * @note            Può fallire e settare errno se si verificano gli errori specificati da malloc(), 
 *                  list_create(), conc_hasht_create(), pthread_mutex_init(), logger_create().
 *                  Nel caso di fallimento di pthread_mutex_init() errno viene settato con i valori che tale funzione ritorna.
 */
storage_t* create_storage(config_t* config, logger_t* logger);

/**
 * @function         destroy_storage()
 * @brief            Distrugge la struttura che rappresenta lo storage deallocando la memoria.
 * 
 * @param storage    Il puntatore alla struttura che rappresenta lo storage da distruggere
 */
void destroy_storage(storage_t* storage);

void open_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id, request_code_t code);

void write_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id, request_code_t code);

void read_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id);

void readn_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id);

void lock_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id);

void unlock_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id);

void remove_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id);

void close_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id);

#endif /* STORAGE_SERVER_H */