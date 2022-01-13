/**
 * @file     storage_server.h
 * @brief    Interfaccia dello storage server.
 */

#ifndef STORAGE_SERVER_H
#define STORAGE_SERVER_H

#include <protocol.h>
#include <config_parser.h>
#include <logger.h>

/* Numero di bytes che costituiscono un MByte */
#define BYTES_IN_A_MEGABYTE 1000000

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
 * @struct              request_t
 * @brief               Struttura che raccoglie gli argomenti di una richiesta
 * 
 * @var code            Codice della richiesta
 * @var file_path       Path del file
 * @var content_size    Size del contenuto del file
 * @var content         Contenuto del file
 * @var n               Valore dell'argomento n
 */
typedef struct request {
	request_code_t code;
	char* file_path;
	size_t content_size;
	void* content;
	int n;
} request_t;

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

/**
 * @function           new_connection_handler()
 * @brief              Registra nello storage il nuovo cliente connesso.
 * 
 * @param storage      Struttura storage
 * @param client_fd    Descrittore del nuovo client connesso
 * 
 * @return             0 in caso di successo, -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                     In caso di fallimento errno può assumere i seguenti valori:
 *                     EINVAL se storage è @c NULL o client_fd è negativo
 *                     EALREADY se client_fd è il descrittore di un cliente già connesso
 */
int new_connection_handler(storage_t* storage, int client_fd);

/**
 * @function           read_request()
 * @brief              Legge la richiesta del cliente associato al descrittore client_fd.
 * 
 * @param storage      Struttura storage
 * @param master_fd    Descrittore del master thread per la comunicazione tra master e workers
 * @param client_fd    Descrittore del cliente che ha effettuato la richiesta
 * @param worker_id    Identificativo del worker che gestisce la richiesta
 * 
 * @return             Un puntatore a una struttura request_t che raccoglie gli argomenti della richiesta in caso di 
 *                     successo, NULL in caso di fallimento con errno settato ad indicare l'errore.
 *                     In caso di fallimento errno può assumere i seguenti valori:
 *                     EINVAL se storage è @c NULL o client_fd è negativo
 *                     ECOMM se la richiesta ricevuta dal cliente non rispetta il protocollo o è stato riscontrato 
 *                     che il cliente si è disconesso
 */
request_t* read_request(storage_t* storage, int master_fd, int client_fd, int worker_id);

/**
 * @function           rejected_task_handler()
 * @brief              Gestisce un task rifiutato dal threadpool.
 * 
 * @param storage      Struttura storage
 * @param master_fd    Descrittore del master thread per la comunicazione tra master e workers
 * @param client_fd    Descrittore del client che ha richiesto il task
 * 
 * @return             1 se il client si è disconnesso, 0 altrimenti.
 */
int rejected_task_handler(storage_t* storage, 
						int master_fd, 
						int client_fd);

int open_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path, 
						request_code_t mode);

int write_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path, 
						void* content, 
						size_t content_size, 
						request_code_t mode);

int read_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path);

int readn_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						int n);

int lock_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path);

int unlock_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path);

int remove_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path);

int close_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path);

/**
 * @function         print_statistics()
 * @brief            Stampa le statistiche sullo stato dello storage.
 * 
 * @param storage    Struttura storage
 * 
 * @return           0 in caso di successo, -1 se storage è @c NULL .
 */
int print_statistics(storage_t* storage);

#endif /* STORAGE_SERVER_H */