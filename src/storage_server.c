/**
 * @file    storage_server.c
 * @brief   Implementazione dello storage server.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

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