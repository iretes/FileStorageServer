/**
 * @file                  storage_server.h
 * @brief                 Interfaccia dello storage server.
 */

#ifndef STORAGE_SERVER_H
#define STORAGE_SERVER_H

#include <protocol.h>
#include <config_parser.h>
#include <logger.h>

/* Numero di bytes che costituiscono un MByte */
#define BYTES_IN_A_MEGABYTE 1000000

/**
 * @def                   LOG()
 * @brief                 Controlla il valore di ritorno di una chiamata di log_record()
 */
#define LOG(X) { \
    if (X == -1) { \
        fprintf(stderr, "Non è stato possibile scrivere sul file di log\n"); \
    } \
}

/* Struttura che rappresenta lo storage */
typedef struct storage storage_t;

/**
 * @struct                request_t
 * @brief                 Struttura che raccoglie gli argomenti di una richiesta
 * 
 * @var code              Codice della richiesta
 * @var file_path         Path del file
 * @var content_size      Size del contenuto del file
 * @var content           Contenuto del file
 * @var n                 Valore dell'argomento n
 */
typedef struct request {
	request_code_t code;
	char* file_path;
	size_t content_size;
	void* content;
	int n;
} request_t;

/**
 * @function              storage_create()
 * @brief                 Inizializza la struttura che rappresenta lo storage e ritorna un puntatore ad essa. 
 *                        Inizializza i campi con i valori iniziali o con i valori dei parametri di configurazione.
 * 
 * @param config          Parametri di configurazione
 * 
 * @return                Un puntatore a una struttura che rappresenta lo storage in caso di successo, 
 *                        NULL in caso di fallimento con errno settato a indicare l'errore.
 *                        In caso di fallimento errno può assumere i seguenti valori:
 *                        EINVAL se config è @c NULL o config->max_file_num <= 0 o config->max_bytes <= 0 o 
 *                        config->max_locks <= 0 o config->expected_clients <= 0
 * @note                  Può fallire e settare errno se si verificano gli errori specificati da malloc(), 
 *                        list_create(), conc_hasht_create(), pthread_mutex_init(), logger_create().
 *                        Nel caso di fallimento di pthread_mutex_init() errno viene settato con i valori che tale funzione
 *                        ritorna.
 */
storage_t* storage_create(config_t* config, logger_t* logger);

/**
 * @function              storage_destroy()
 * @brief                 Distrugge la struttura che rappresenta lo storage deallocando la memoria.
 * 
 * @param storage         Il puntatore alla struttura che rappresenta lo storage da distruggere
 */
void storage_destroy(storage_t* storage);

/**
 * @function              new_connection_handler()
 * @brief                 Registra nello storage il nuovo cliente connesso.
 * 
 * @param storage         Struttura storage
 * @param client_fd       Descrittore del nuovo client connesso
 * 
 * @return                0 in caso di successo, -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                        In caso di fallimento errno può assumere i seguenti valori:
 *                        EINVAL se storage è @c NULL o client_fd è negativo
 *                        EALREADY se client_fd è il descrittore di un cliente già connesso
 */
int new_connection_handler(storage_t* storage, int client_fd);

/**
 * @function              read_request()
 * @brief                 Legge la richiesta del cliente associato al descrittore client_fd.
 * 
 * @param storage         Struttura storage
 * @param master_fd       Descrittore del master thread per la comunicazione tra master e workers
 * @param client_fd       Descrittore del cliente che ha effettuato la richiesta
 * @param worker_id       Identificativo del worker che gestisce la richiesta
 * 
 * @return                Un puntatore a una struttura request_t che raccoglie gli argomenti della richiesta in caso di 
 *                        successo, NULL in caso di fallimento con errno settato ad indicare l'errore.
 *                        In caso di fallimento errno può assumere i seguenti valori:
 *                        EINVAL se storage è @c NULL o client_fd è negativo
 *                        ECOMM se la richiesta ricevuta dal cliente non rispetta il protocollo o è stato riscontrato 
 *                        che il cliente si è disconesso
 */
request_t* read_request(storage_t* storage, int master_fd, int client_fd, int worker_id);

/**
 * @function              rejected_task_handler()
 * @brief                 Gestisce un task rifiutato dal threadpool.
 * 
 * @param storage         Struttura storage
 * @param master_fd       Descrittore del master thread per la comunicazione tra master e workers
 * @param client_fd       Descrittore del client che ha richiesto il task
 * 
 * @return                1 se il client si è disconnesso, 0 altrimenti.
 */
int rejected_task_handler(storage_t* storage, 
						int master_fd, 
						int client_fd);

/**
 * @function              open_file_handler()
 * @brief                 Serve la richiesta di apertura di un file.
 *                        Se riscontra che client_fd si è disconesso scrive a master_fd -(client_fd), 
 *                        altrimenti scrive client_fd.
 * 
 * @param storage         Struttura storage
 * @param master_fd       Descrittore del master thread per la comunicazione tra master e workers
 * @param client_fd       Descrittore del client che ha effettuato la richiesta
 * @param worker_id       Identificato del worker che serve la richiesta
 * @param file_path       Path del file da aprire
 * @param mode            Modalità di aperura (OPEN_NO_FLAGS | OPEN_CREATE | OPEN_LOCK | OPEN_CREATE_LOCK)
 * 
 * @return                0 in caso di successo, -1 se storage è @c NULL , master_fd o client_fd sono negativi, file_path è 
 *                        @c NULL o la sua lunghezza è 0 o mode è diversa da OPEN_NO_FLAGS, OPEN_CREATE, OPEN_LOCK e 
 *                        OPEN_CREATE_LOCK.
 */
int open_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path, 
						request_code_t mode);

/**
 * @function              write_file_handler()
 * @brief                 Serve la richiesta di write o append di un file.
 *                        Se riscontra che client_fd si è disconesso scrive a master_fd -(client_fd), 
 *                        altrimenti scrive client_fd.
 * 
 * @param storage         Struttura storage
 * @param master_fd       Descrittore del master thread per la comunicazione tra master e workers
 * @param client_fd       Descrittore del client che ha effettuato la richiesta
 * @param worker_id       Identificato del worker che serve la richiesta
 * @param file_path       Path del file da scrivere
 * @param content         Contenuto del file da scrivere
 * @param content_size    Size del file da scrivere
 * @param mode            Modalità di scrittura (WRITE | APPEND)
 * 
 * @return                0 in caso di successo, -1 se storage è @c NULL , master_fd o client_fd sono negativi, file_path è 
 *                        @c NULL o la sua lunghezza è 0 o mode è diversa da WRITE e APPEND.
 */
int write_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path, 
						void* content, 
						size_t content_size, 
						request_code_t mode);

/**
 * @function              read_file_handler()
 * @brief                 Serve la richiesta di read di un file.
 *                        Se riscontra che client_fd si è disconesso scrive a master_fd -(client_fd), 
 *                        altrimenti scrive client_fd.
 * 
 * @param storage         Struttura storage
 * @param master_fd       Descrittore del master thread per la comunicazione tra master e workers
 * @param client_fd       Descrittore del client che ha effettuato la richiesta
 * @param worker_id       Identificato del worker che serve la richiesta
 * @param file_path       Path del file da leggere
 * 
 * @return                0 in caso di successo, -1 se storage è @c NULL , master_fd o client_fd sono negativi, file_path è 
 *                        @c NULL o la sua lunghezza è 0.
 */
int read_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path);

/**
 * @function              readn_file_handler()
 * @brief                 Serve la richiesta di readn.
 *                        Se riscontra che client_fd si è disconesso scrive a master_fd -(client_fd), 
 *                        altrimenti scrive client_fd.
 * 
 * @param storage         Struttura storage
 * @param master_fd       Descrittore del master thread per la comunicazione tra master e workers
 * @param client_fd       Descrittore del client che ha effettuato la richiesta
 * @param worker_id       Identificativo del worker che serve la richiesta
 * @param n               Numero di file da leggere, se <= 0 indica una richiesta di lettura di tutti i file (leggibili) 
 *                        dello storage
 * 
 * @return                0 in caso di successo, -1 se storage è @c NULL , master_fd o client_fd sono negativi.
 */
int readn_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						int n);

/**
 * @function              lock_file_handler()
 * @brief                 Serve la richiesta di lock di un file.
 *                        Se riscontra che client_fd si è disconesso scrive a master_fd -(client_fd), 
 *                        altrimenti scrive client_fd.
 * 
 * @param storage         Struttura storage
 * @param master_fd       Descrittore del master thread per la comunicazione tra master e workers
 * @param client_fd       Descrittore del client che ha effettuato la richiesta
 * @param worker_id       Identificativo del worker che serve la richiesta
 * @param file_path       Path del file su cui efferruare l'operazione di lock
 * 
 * @return                0 in caso di successo, -1 se storage è @c NULL , master_fd o client_fd sono negativi, file_path è 
 *                        @c NULL o la sua lunghezza è 0.
 */
int lock_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path);

/**
 * @function              unlock_file_handler()
 * @brief                 Server la richiesta di unlock di un file.
 *                        Se riscontra che client_fd si è disconesso scrive a master_fd -(client_fd), 
 *                        altrimenti scrive client_fd.
 * 
 * @param storage         Struttura storage
 * @param master_fd       Descrittore del master thread per la comunicazione tra master e workers
 * @param client_fd       Descrittore del client che ha effettuato la richiesta
 * @param worker_id       Identificativo del worker che serve la richiesta
 * @param file_path       Path del file su cui effettuare l'operazione di unlock
 * 
 * @return                0 in caso di successo, -1 se storage è @c NULL , master_fd o client_fd sono negativi, file_path è 
 *                        @c NULL o la sua lunghezza è 0.
 */
int unlock_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path);

/**
 * @function              remove_file_handler()
 * @brief                 Server la richiesta di remove di un file. 
 *                        Se riscontra che client_fd si è disconesso scrive a master_fd -(client_fd), 
 *                        altrimenti scrive client_fd.
 * 
 * @param storage         Struttura storage
 * @param master_fd       Descrittore del master thread per la comunicazione tra master e workers
 * @param client_fd       Descrittore del client che ha effettuato la richiesta
 * @param worker_id       Identificativo del worker che serve la richiesta
 * @param file_path       Path del file da rimuovere
 * 
 * @return                0 in caso di successo, -1 se storage è @c NULL , master_fd o client_fd sono negativi, file_path è 
 *                        @c NULL o la sua lunghezza è 0.
 */
int remove_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path);

/**
 * @function              close_file_handler()
 * @brief                 Server la richiesta di close di un file.
 *                        Se riscontra che client_fd si è disconesso scrive a master_fd -(client_fd), 
 *                        altrimenti scrive client_fd.
 * 
 * @param storage         Struttura storage
 * @param master_fd       Descrittore del master thread per la comunicazione tra master e workers
 * @param client_fd       Descrittore del client che ha effettuato la richiesta
 * @param worker_id       Identificativo del worker che serve la richiesta
 * @param file_path       Path del file da chiudere
 * 
 * @return                0 in caso di successo, -1 se storage è @c NULL , master_fd o client_fd sono negativi, file_path è 
 *                        @c NULL o la sua lunghezza è 0.
 */
int close_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path);

/**
 * @function              print_statistics()
 * @brief                 Stampa le statistiche sullo stato dello storage.
 * 
 * @param storage         Struttura storage
 * 
 * @return                0 in caso di successo, -1 se storage è @c NULL .
 */
int print_statistics(storage_t* storage);

#endif /* STORAGE_SERVER_H */