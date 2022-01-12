/**
 * @file    storage_server.c
 * @brief   Implementazione dello storage server.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

#include <storage_server.h>
#include <config_parser.h>
#include <list.h>
#include <int_list.h>
#include <conc_hasht.h>
#include <eviction_policy.h>
#include <util.h>

/**
 * @struct                   storage_t
 * @brief                    Struttura che rappresenta lo storage.
 * 
 * @var max_files            Numero massimo di file memorizzabili
 * @var max_bytes            Numero massimo di byte memorizzabili
 * @var curr_file_num        Numero corrente di file memorizzati
 * @var curr_bytes           Numero corrente di file memorizzati
 * @var max_files_stored     Numero massimo di file memorizzati
 * @var max_bytes_stored     Numero massimo di bytes memorizzati
 * @var evicted_files        Numero di file espulsi
 * @var eviction_policy      Politica di espulsione dei file dallo storage
 * @var files_queue          Coda dei file memorizzati
 * @var files_ht             Tabella hash thread safe per i file memorizzati
 * @var connected_clients    Tabella hash thread safe per i clienti connessi
 * @var mutex                Mutex per l'accesso in mutua esclusione allo storage
 * @var logger               Oggetto logger
 */
typedef struct storage {
	size_t max_files;
	size_t max_bytes;
	size_t curr_file_num;
	size_t curr_bytes;
	size_t max_files_stored;
	size_t max_bytes_stored;
	size_t evicted_files;
	eviction_policy_t eviction_policy;
	list_t* files_queue;
	conc_hasht_t* files_ht;
	conc_hasht_t* connected_clients;
	pthread_mutex_t mutex;
	logger_t* logger;
} storage_t;

/**
 * @struct                  file_t
 * @brief                   Struttura che rappresenta un file nello storage.
 * 
 * @var path                Path del file
 * @var content             Contenuto del file
 * @var content_size        Dimensione del contenuto del file
 * @var locked_by_fd        File descriptor del client che è in possesso della lock sul file
 * @var can_write_fd        File descriptor del client che può effettuare l'operazione write, -1 se nessun client ha tale diritto
 * @var pending_lock_fds    Lista dei file descriptor dei client che sono in attesa di acquisire la lock sul file
 * @var open_by_fds         Lista dei file descriptor dei client che hanno aperto il file
 * @var creation_time       Timestamp della creazione del file
 * @var last_usage_time     Timestamp dell'ultimo utilizzo del file
 * @var usage_counter       Contatore degli utilizzi del file
 */
typedef struct file {
	char* path;
	void* content;
	size_t content_size;
	int locked_by_fd;
	int can_write_fd;
	int_list_t* pending_lock_fds;
	int_list_t* open_by_fds;
	struct timespec creation_time;
	struct timespec last_usage_time;
	int usage_counter;
} file_t;

/**
 * @struct              client_t
 * @brief               Struttura che rappresenta un client.
 *
 * @var fd              Descrittore del client connesso al server
 * @var opened_files    Lista dei file aperti dal client
 * @var locked_files    Lista dei file bloccati dal client
 */
typedef struct client {
	int fd;
	list_t* opened_files;
	list_t* locked_files;
} client_t;

/**
 * @def           WRITE_TO_CLIENT()
 * @brief         Scrive al file descriptor fd il buffer buf, memorizza il valore ritornato dalla write in r e stampa 
 *                eventualmente sullo stderr l'errore verificatosi.
 * 
 * @param fd      File descriptor del cliente
 * @param buf     Riferimento al buffer da scrivere
 * @param size    Dimensione del buffer buf
 * @param r       Valore ritornato dalla scrittura
 */
#define WRITE_TO_CLIENT(fd, buf, size, r) \
	do { \
		if ((r = writen(fd, buf, size)) == -1 && errno != EPIPE) { \
			PERRORSTR(errno) \
		} \
	} while(0);

/**
 * @def            READ_FROM_CLIENT()
 * @brief          Legge dal file descriptor fd nel buffer buf, memorizza il valore ritornato dalla read in r e stampa 
 *                 eventualmente sullo stderr l'errore verificatosi.
 * 
 * @param fd       File descriptor del cliente
 * @param buf      Riferimento al buffer da leggere
 * @param size     Dimensione del buffer buf
 * @param r        Valore ritornato dalla scrittura
 */
#define READ_FROM_CLIENT(fd, buf, size, r) \
	do { \
		if ((r = readn(fd, buf, size)) == -1 && errno != ECONNRESET) { \
			PERRORSTR(errno) \
		} \
	} while(0);

/**
 * @function     send_response_code()
 * @brief        Invia al cliente associato al file descriptor fd il codice di risposta code
 * 
 * @param fd     File descriptor del cliente
 * 
 * @return       0 in caso di sucesso, -1 in caso di fallimento ed errno settato ad indicare l'errore
 * @note         Errno viene eventualmente settato da writen()
 */
static int send_response_code(int fd, response_code_t code) {
	int r;
	WRITE_TO_CLIENT(fd, &code, sizeof(response_code_t), r);
	if (r == -1 || r == 0)
		return -1;
	return 0;
}

/**
 * @function      init_file()
 * @brief         Inizializza una struttura che rappresenta un file nello storage e ritorna un puntatore ad essa.
 * 
 * @param path    Path del file
 * 
 * @return        Un puntatore alla struttura che rappresenta un file nello storage in caso di successo,
 *                NULL in caso di fallimento ed errno settato ad indicare l'errore.
 *                In caso di fallimento errno può assumere i seguenti valori:
 *                EINVAL se path è @c NULL o la sua lunghezza è 0
 * @note          Può fallire e settare errno se si verificano gli errori specificati da malloc() e int_list_create().
 */
static file_t* init_file(char* path) {
	if (path == NULL || strlen(path) == 0) {
		errno = EINVAL;
		return NULL;
	}
	file_t* file = malloc(sizeof(file_t));
	if (file == NULL)
		return NULL;
	
	file->path = path;
	file->content = NULL;
	file->content_size = 0;
	file->locked_by_fd = -1;
	file->can_write_fd = -1;

	file->pending_lock_fds = int_list_create();
	if (file->pending_lock_fds == NULL) {
		free(file);
		return NULL;
	}		
	file->open_by_fds = int_list_create();
	if (file->open_by_fds == NULL) {
		free(file);
		int_list_destroy(file->pending_lock_fds);
		return NULL;
	}
	
	clock_gettime(CLOCK_REALTIME, &file->creation_time);
	file->last_usage_time = file->creation_time;
	file->usage_counter = 0;

	return file;
}

/**
 * @function      destroy_file()
 * @brief         Distrugge la struttura che rappresenta un file nello storage.
 * 
 * @param file    Puntatore alla struttura che rappresenta il file da distruggere
 */
static void destroy_file(file_t* file) {
	if (!file)
		return;
	if (file->path)
		free(file->path);
	if (file->content)
		free(file->content);
	if (file->pending_lock_fds)
		int_list_destroy(file->pending_lock_fds);
	if (file->open_by_fds != NULL)
		int_list_destroy(file->open_by_fds);
	free(file);
	file = NULL;
}

/**
 * @function    cmp_file()
 * @brief       Paragona due strutture che rappresentano file.
 *              Due file sono uguali se i loro path sono uguali.
 * 
 * @param a     Primo oggetto file da confrontare
 * @param b     Secondo oggetto file da confrontare
 * 
 * @return      1 se i file sono uguali, 0 altrimenti.
 */
static int cmp_file(void* a, void* b) {
	if (!a && !b) 
		return 1;
	else if (!a || !b)
		return 0;
	file_t* f1 = a;
	file_t* f2 = b;
	return (strcmp(f1->path, f2->path) == 0);
}

/**
 * @function    init_client()
 * @brief       Inizializza una struttura che rappresenta un client e ritorna un puntatore ad essa.
 * 
 * @param fd    Descrittore del client connesso al server
 * 
 * @return      Un puntatore a una struttura che rappresenta un client connesso al server in caso di successo,
 *              NULL in caso di fallimento ed errno settato ad indicare l'errore.
 *              In caso di fallimento errno può assumere i seguenti valori:
 *              EINVAL es fd è < 0
 * @note        Può fallire e settare errno se si verificano gli errori specificati da malloc() e list_create().
 */
static client_t* init_client(int fd) {
	if (fd < 0) {
		errno = EINVAL;
		return NULL;
	}
	client_t* client = malloc(sizeof(client_t));
	if (!client)
		return NULL;
		
	client->fd = fd;

	client->opened_files = list_create(cmp_file, (void (*)(void*)) destroy_file);
	if (!client->opened_files) {
		free(client);
		return NULL;
	}
	client->locked_files = list_create(cmp_file, (void (*)(void*)) destroy_file);
	if (!client->locked_files) {
		free(client);
		list_destroy(client->opened_files, LIST_DO_NOT_FREE_DATA);
	}

	return client;
}

/**
 * @function        destroy_client()
 * @brief           Distrugge la struttura che rappresenta un client deallocando la memoria.
 *
 * @param client    Il puntatore alla struttura che rappresenta il client da distruggere
 *
 */
static void destroy_client(client_t* client) {
	if (!client)
		return;
	if (client->opened_files)
		list_destroy(client->opened_files, LIST_DO_NOT_FREE_DATA);
	if (client->locked_files)
		list_destroy(client->locked_files, LIST_DO_NOT_FREE_DATA);
	free(client);
	client = NULL;
}

storage_t* create_storage(config_t* config, logger_t* logger) {
	if (!config || config->max_file_num <= 0 || config->max_bytes <= 0 || 
		config->max_locks <= 0 || config->expected_clients <= 0) {
		errno = EINVAL;
		return NULL;
	}
	storage_t* storage = malloc(sizeof(storage_t));
	if (!storage)
		return NULL;
		
	storage->max_files = config->max_file_num;
	storage->max_bytes = config->max_bytes;
	storage->curr_file_num = 0;
	storage->curr_bytes = 0;
	storage->max_files_stored = 0;
	storage->max_bytes_stored = 0;
	storage->evicted_files = 0;
	storage->eviction_policy = config->eviction_policy;

	storage->files_queue = list_create(cmp_file, (void (*)(void*)) destroy_file);
	if (!storage->files_queue) {
		free(storage);
		return NULL;
	}

	int errnosv;
	int file_buckets = (storage->max_files) / LOAD_FACTOR;
	config->max_locks = (config->max_locks) / LOAD_FACTOR;
	storage->files_ht = conc_hasht_create(file_buckets, config->max_locks, NULL, NULL);
	if (!storage->files_ht) {
		errnosv = errno;
		list_destroy(storage->files_queue, LIST_DO_NOT_FREE_DATA);
		free(storage);
		errno = errnosv;
		return NULL;
	}

	int client_buckets = (config->expected_clients) / LOAD_FACTOR;
	storage->connected_clients = conc_hasht_create(client_buckets, client_buckets, NULL, int_cmp);
	if (!storage->connected_clients) {
		errnosv = errno;
		list_destroy(storage->files_queue, LIST_DO_NOT_FREE_DATA);
		conc_hasht_destroy(storage->files_ht, NULL, (void (*)(void*)) destroy_file);
		free(storage);
		errno = errnosv;
		return NULL;
	}

	int r;
	r = pthread_mutex_init(&(storage->mutex), NULL);
	if (r != 0) {
		list_destroy(storage->files_queue, LIST_DO_NOT_FREE_DATA);
		conc_hasht_destroy(storage->files_ht, NULL, NULL);
		conc_hasht_destroy(storage->connected_clients, NULL, (void (*)(void*)) destroy_client);
		free(storage);
		errno = r;
		return NULL;
	}

	storage->logger = logger;

	return storage;
}

void destroy_storage(storage_t* storage) {
	if (!storage)
		return;
	if (storage->files_queue)
		list_destroy(storage->files_queue, LIST_DO_NOT_FREE_DATA);
	if (storage->files_ht)
		conc_hasht_destroy(storage->files_ht, NULL, (void (*)(void*)) destroy_file);
	if (storage->connected_clients)
		conc_hasht_destroy(storage->connected_clients, NULL, (void (*)(void*)) destroy_client);
	pthread_mutex_destroy(&(storage->mutex));
	free(storage);
	storage = NULL;
}

int new_connection_handler(storage_t* storage, int client_fd) {
	if (storage == NULL || client_fd < 0) {
		errno = EINVAL;
		return -1;
	}

	int r;
	// creo un oggetto cliente
	client_t* client = NULL;
	EQNULL_DO(init_client(client_fd), client, EXTF);

	// aggiungo il client alla tabella hash di clienti connessi se non è già presente
	EQM1_DO(conc_hasht_lock(storage->connected_clients, &client_fd), r, EXTF);
	EQM1_DO(conc_hasht_contains(storage->connected_clients, &client_fd), r, EXTF);
	if (r) {
		EQM1_DO(conc_hasht_unlock(storage->connected_clients, &client_fd), r, EXTF);
		errno = EALREADY;
		return -1;
	}
	EQM1_DO(conc_hasht_insert(storage->connected_clients, &client->fd, client), r, EXTF);
	EQM1_DO(conc_hasht_unlock(storage->connected_clients, &client_fd), r, EXTF);

	return 0;
}

static void close_client_connection(storage_t* storage, int master_fd, int client_fd, int worker_id) {
	int r;

	// elimino il client dallo storage
	client_t* client;
	ERRNOSET_DO(conc_hasht_atomic_delete_and_get(storage->connected_clients, &client_fd, NULL), client, EXTF);

	// comunico al master che il cliente si è disconnesso
	int neg_client_fd = -client_fd;
	EQM1_DO(writen(master_fd, &neg_client_fd, sizeof(int)), r, EXTF);

	EQM1(close(client_fd), r);
	destroy_client(client);
}

request_t* read_request(storage_t* storage, int master_fd, int client_fd, int worker_id) {
	if (storage == NULL || client_fd < 0) {
		errno = EINVAL;
		return NULL;
	}
	
	int r;

	// struttura per memorizzare gli argomenti della richiesta
	request_t* req = NULL;
	EQNULL_DO(malloc(sizeof(request_t)), req, EXTF);
	req->code = -1;
	req->file_path = NULL;
	req->content_size = 0;
	req->content = NULL;
	req->n = 0;

	// leggo il codice della richiesta
	READ_FROM_CLIENT(client_fd, &req->code, sizeof(request_code_t), r);
	if (r == -1 || r == 0) {
		close_client_connection(storage, master_fd, client_fd, worker_id);
		free(req);
		errno = ECOMM;
		return NULL;
	}
	// controllo che il codice di richiesta sia valido
	if (req->code < MIN_REQ_CODE || req->code > MAX_REQ_CODE) {
		LOG(log_record(storage->logger, "%d,%s,%s,%d,,%d",
			worker_id, NULL, resp_code_to_str(NOT_RECOGNIZED_OP), client_fd, 0));
		send_response_code(client_fd, NOT_RECOGNIZED_OP);
		close_client_connection(storage, master_fd, client_fd, worker_id);
		free(req);
		errno = ECOMM;
		return NULL;
	}

	if (req->code != READN) {
		size_t file_path_len;
		// leggo la size del path del file
		READ_FROM_CLIENT(client_fd, &file_path_len, sizeof(size_t), r);
		if (r == -1 || r == 0) {
			close_client_connection(storage, master_fd, client_fd, worker_id);
			free(req);
			errno = ECOMM;
			return NULL;
		}
		// controllo se il path è troppo lungo
		if (file_path_len > (PATH_MAX-1)) {
			LOG(log_record(storage->logger, "%d,%s,%s,%d,,%d",
				worker_id, req_code_to_str(req->code), resp_code_to_str(TOO_LONG_PATH), client_fd, 0));
			send_response_code(client_fd, TOO_LONG_PATH);
			close_client_connection(storage, master_fd, client_fd, worker_id);
			free(req);
			errno = ECOMM;
			return NULL;
		}
		// controllo se il path è vuoto
		if (file_path_len == 0) {
			LOG(log_record(storage->logger, "%d,%s,%s,%d,,%d",
				worker_id, req_code_to_str(req->code), resp_code_to_str(INVALID_PATH), client_fd, 0));
			send_response_code(client_fd, INVALID_PATH);
			close_client_connection(storage, master_fd, client_fd, worker_id);
			free(req);
			errno = ECOMM;
			return NULL;
		}

		// leggo il path del file
		EQNULL_DO(calloc(file_path_len, sizeof(char)), req->file_path, EXTF);
		READ_FROM_CLIENT(client_fd, req->file_path, sizeof(char)*(file_path_len), r);
		if (r == -1 || r == 0) {
			close_client_connection(storage, master_fd, client_fd, worker_id);
			free(req->file_path);
			free(req);
			errno = ECOMM;
			return NULL;
		}
		// controllo che sia un path valido (non contenga ',' e finisca con '\0')
		if (req->file_path[file_path_len-1] != '\0' || strchr(req->file_path, ',') != NULL) {
			LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
				worker_id, req_code_to_str(req->code), resp_code_to_str(INVALID_PATH), client_fd, req->file_path, 0));
			send_response_code(client_fd, INVALID_PATH);
			close_client_connection(storage, master_fd, client_fd, worker_id);
			free(req->file_path);
			free(req);
			errno = ECOMM;
			return NULL;
		}
		// controllo che sia un path assoluto
		if (strchr(req->file_path, '/') != req->file_path) {
			LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
				worker_id, req_code_to_str(req->code), resp_code_to_str(INVALID_PATH), client_fd, req->file_path, 0));
			send_response_code(client_fd, INVALID_PATH);
			close_client_connection(storage, master_fd, client_fd, worker_id);
			free(req->file_path);
			free(req);
			errno = ECOMM;
			return NULL;
		}
	}

	if (req->code == WRITE || req->code == APPEND) {
		// leggo la size del contenuto del file
		READ_FROM_CLIENT(client_fd, &req->content_size, sizeof(size_t), r);
		if (r == -1 || r == 0) {
			close_client_connection(storage, master_fd, client_fd, worker_id);
			free(req->file_path);
			free(req);
			errno = ECOMM;
			return NULL;
		}
		// controllo che la dimensione del file non sia maggiore della capacità dello storage
		if (req->content_size > storage->max_bytes) {
			LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
				worker_id, req_code_to_str(req->code), resp_code_to_str(TOO_LONG_CONTENT), client_fd, req->file_path, 0));
			send_response_code(client_fd, TOO_LONG_CONTENT);
			close_client_connection(storage, master_fd, client_fd, worker_id);
			free(req->file_path);
			free(req);
			errno = ECOMM;
			return NULL;
		}
		if (req->content_size != 0) {
			// leggo il contenuto del file
			EQNULL_DO(malloc(req->content_size), req->content, EXTF);
			READ_FROM_CLIENT(client_fd, req->content, req->content_size, r);
			if (r == -1 || r == 0) {
				close_client_connection(storage, master_fd, client_fd, worker_id);
				free(req->file_path);
				free(req->content);
				free(req);
				errno = ECOMM;
				return NULL;
			}
		}
	}

	if (req->code == READN) {
		// leggo il valore di n
		READ_FROM_CLIENT(client_fd, &req->n, sizeof(int), r);
		if (r == -1 || r == 0) {
			close_client_connection(storage, master_fd, client_fd, worker_id);
			free(req->file_path);
			free(req);
			errno = ECOMM;
			return NULL;
		}
	}

	return req;
}

void open_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id, request_code_t code) {

}

void write_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id, request_code_t code) {

}

void read_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id) {

}

void readn_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id) {

}

void lock_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id) {

}

void unlock_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id) {

}

void remove_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id) {

}

void close_file_handler(storage_t* storage, int master_fd, int client_fd, int worker_id) {

}