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
#include <logger.h>
#include <log_format.h>

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
 * @function       send_size()
 * @brief          Invia al cliente associato al file descriptor fd size
 * 
 * @param fd       File descriptor del cliente
 * @param size     Valore da inviare
 * 
 * @return         0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore
 * @note           Errno viene eventualmente settato da writen()
 */
static int send_size(int fd, size_t size) {
	int r;
	WRITE_TO_CLIENT(fd, &size, sizeof(size_t), r);
	if (r == -1 || r == 0)
		return -1;
	return 0;
}

/**
 * @function           send_file_name()
 * @brief              Invia al cliente associato al file descriptor fd la dimensione del path path_size e path
 * 
 * @param fd           File descriptor del cliente
 * @param path_size    Dimensione del path 
 * @param path         Path del file
 * 
 * @return             0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore
 * @note               Errno viene eventualmente settato da writen() o da send_size()
 */
static int send_file_name(int fd, size_t path_size, char* path) {
	int r;
	if (send_size(fd, path_size) == -1)
		return -1;
	WRITE_TO_CLIENT(fd, path, path_size*sizeof(char), r);
	if (r == -1 || r == 0)
		return -1;
	return 0;
}

/**
 * @function              send_file_content()
 * @brief                 Invia al cliente associato al file descriptor fd la dimensione del file file_size e il contenuto 
 *                        del file file_content
 * 
 * @param fd              File descriptor del cliente
 * @param file_size       Dimensione del file
 * @param file_content    Contenuto del file
 * 
 * @return                0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore
 * @note                  Errno viene eventualmente settato da writen() o da send_size()
 */
static int send_file_content(int fd, size_t file_size, void* file_content) {
	int r = 0;
	if (send_size(fd, file_size) == -1)
		return -1;
	if (file_size != 0) {
		WRITE_TO_CLIENT(fd, file_content, file_size, r);
		if (r == -1 || r == 0)
			return -1;
	}
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

/**
 * @function           give_lock_to_waiting_client()
 * @brief              Estrae il primo cliente in attesa di acquisire la lock su file e gli assegna la lock.
 *                     Se nessun client è in attesa di acquisire la lock setta il campo file->locked_by_fd a -1.
 * @warning            Questa funzione deve essere invocata dopo aver acquisito la lock sulla tabella hash di file per 
 *                     l'accesso a file.
 * 
 * @param storage      Oggetto storage
 * @param file         File il cui detentore della lock deve essere modificato
 * @param worker_id    Identificativo del worker thread che gestisce la richiesta
 * 
 * @return             Il descrittore del client la cui connessione dovrà essere chiusa perchè disconnesso,
 *                     -1 se nessun client si è disconnesso.
 */
static int give_lock_to_waiting_client(storage_t* storage, file_t* file, int worker_id, int master_fd) {
	int r, fd;

	// rimuovo il primo cliente in attesa di acquisire la lock su file
	ERRNOSET_DO(int_list_head_remove(file->pending_lock_fds, &fd), r, EXTF);
	if (r == -1) {
		// nessun cliente è in attesa di acquisire la lock
		file->locked_by_fd = -1;
		return -1;
	}

	file->locked_by_fd = fd;

	// recupero l'oggetto relativo al client divenuto il detentore della lock
	client_t* client;
	EQM1_DO(conc_hasht_lock(storage->connected_clients, &fd), r, EXTF);
	ERRNOSET_DO(conc_hasht_get_value(storage->connected_clients, &fd), client, EXTF);
	if (client == NULL) {
		EQM1_DO(conc_hasht_unlock(storage->connected_clients, &fd), r, EXTF);
		return -1;
	}
	// inserisco il file nella lista di file bloccati dal cliente
	EQM1_DO(list_tail_insert(client->locked_files, file), r, EXTF);
	EQM1_DO(conc_hasht_unlock(storage->connected_clients, &fd), r, EXTF);

	LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d", 
		worker_id, OP_SUSPENDED, resp_code_to_str(OK), fd, file->path, 0));

	// rispondo al client in attesa della lock l'esito positivo dell'operazione
	if (send_response_code(fd, OK) == -1)
		return fd;
	
	// comunico al master di aver servito il cliente che era in attesa
	EQM1_DO(writen(master_fd, &fd, sizeof(int)), r, EXTF);
	
	return -1;
}

/**
 * @function           delete_client_from_storage()
 * @brief              Elimina il client dallo storage.
 * @warning            Questa funzione deve essere invocata senza avere alcuna lock acquisita.
 * 
 * @param storage      Oggetto storage
 * @param master_fd    Descrittore per la comunicazione con il master thread
 * @param client_fd    Descrittore del client di cui chiudere la connessione
 * @param worker_id    Identificativo del worker thread che gestisce la richiesta
 * 
 * @return             La lista dei descrittori dei client le cui connessioni devono essere chiuse perchè disconnessi.
 */
static int_list_t* delete_client_from_storage(storage_t* storage, int master_fd, int client_fd, int worker_id) {
	int r;

	// lista dei clienti di cui dovrò chiudere la connessione
	int_list_t* clients_unreachable = NULL;
	EQNULL_DO(int_list_create(), clients_unreachable, EXTF);
	
	// prendo la lock sullo storage per evitare che vengano ad esempio cancellati file aperti o bloccati dal client
	NEQ0_DO(pthread_mutex_lock(&storage->mutex), r, EXTF);

	client_t* client;
	
	ERRNOSET_DO(conc_hasht_atomic_delete_and_get(storage->connected_clients, &client_fd, NULL), client, EXTF);
	// le risorse associate al cliente sono già state deallocate
	if (client == NULL) {
		NEQ0_DO(pthread_mutex_unlock(&storage->mutex), r, EXTF);
		return clients_unreachable;
	}

	file_t* file;
	// itero sui file bloccati dal cliente
	list_for_each(client->locked_files, file) {
		if (file != NULL && file->path != NULL) {
			EQM1_DO(conc_hasht_lock(storage->files_ht, file->path), r, EXTF); 
			// passo la lock sul file a un eventuale cliente in attesa
			int fd = give_lock_to_waiting_client(storage, file, worker_id, master_fd);
			/* se nel contattare il client a cui passare la lock ho riscontrato che si è disconnesso
			   aggiungo il suo descrittore alla lista di clienti di cui dovrò chiudere la connessione */
			if (fd != -1)
				EQM1_DO(int_list_tail_insert(clients_unreachable, fd), r, EXTF);
			EQM1_DO(conc_hasht_unlock(storage->files_ht, file->path), r, EXTF);
		}
	}

	// itero sui file aperti dal cliente
	list_for_each(client->opened_files, file) {
		if (file != NULL && file->path != NULL) {
			EQM1_DO(conc_hasht_lock(storage->files_ht, file->path), r, EXTF);
			// rimuovo il cliente dalla lista di descrittori di clienti che hanno aperto il file
			EQM1_DO(int_list_remove(file->open_by_fds, client_fd), r, EXTF);
			EQM1_DO(conc_hasht_unlock(storage->files_ht, file->path), r, EXTF);
		}
	}

	NEQ0_DO(pthread_mutex_unlock(&storage->mutex), r, EXTF);

	// comunico al master che il cliente si è disconnesso
	int neg_client_fd = -client_fd;
	EQM1_DO(writen(master_fd, &neg_client_fd, sizeof(int)), r, EXTF);

	destroy_client(client);

	return clients_unreachable;
}

/**
 * @function           close_client_connection()
 * @brief              Chiude la connessione del cliente liberando le risorse associate.
 * @warning            Questa funzione deve essere invocata senza avere alcuna lock acquisita.
 * 
 * @param storage      Oggetto storage
 * @param master_fd    Descrittore per la comunicazione con il master thread
 * @param client_fd    Descrittore del client di cui chiudere la connessione
 * @param worker_id    Identificativo del worker thread che gestisce la richiesta
 */
void close_client_connection(storage_t* storage, int master_fd, int client_fd, int worker_id) {
	int r, fd;

	// lista dei descrittori dei clienti di cui chiudere la connessione
	int_list_t* clients_to_close = NULL;
	EQNULL_DO(int_list_create(), clients_to_close, EXTF);
	EQM1_DO(int_list_tail_insert(clients_to_close, client_fd), r, EXTF);

	// lista di clienti che si sono disconnessi
	int_list_t* clients_unreachable;
	// itero fino a che la lista non è vuota
	EQM1_DO(int_list_is_empty(clients_to_close), r, EXTF);
	while (!r) {
		// estraggo il descrittore di un client
		EQM1_DO(int_list_head_remove(clients_to_close, &fd), r, EXTF);
		// libero le risorse associate alla connessione del client
		clients_unreachable = delete_client_from_storage(storage, master_fd, fd, worker_id);
		// concateno alla lista di clienti di cui chiudere la connessione la lista di clienti disconnessi
		EQM1_DO(int_list_concatenate(clients_to_close, clients_unreachable), r, EXTF);
		int_list_destroy(clients_unreachable);
		EQM1_DO(int_list_is_empty(clients_to_close), r, EXTF);
	}
	int_list_destroy(clients_to_close);
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
		if (file_path_len > PATH_MAX) {
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
		// controllo che sia un path valido (non contenga ',' finisca con '\0' e inizi con '/')
		if (req->file_path[file_path_len-1] != '\0' ||
			strchr(req->file_path, ',') != NULL ||
			strchr(req->file_path, '/') != req->file_path) {
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

int rejected_task_handler(storage_t* storage, 
						int master_fd, 
						int client_fd) {
	// flag che indica se il client si è disconnesso
	int disconnected = 0;
	// leggo la richiesta del client
	request_t* req = read_request(storage, master_fd, client_fd, MASTER_ID);
	if (req == NULL) {
		disconnected = 1;
		return disconnected;
	}

	LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
		MASTER_ID, 
		req_code_to_str(req->code), 
		resp_code_to_str(TEMPORARILY_UNAVAILABLE), 
		client_fd, req->file_path, 
		0));

	// rispondo al cliente comunicando che il server è momentaneamente non disponibile
	if (send_response_code(client_fd, TEMPORARILY_UNAVAILABLE) == -1) {
		close_client_connection(storage, master_fd, client_fd, MASTER_ID);
		disconnected = 1;
	}
	if (req->file_path)
		free(req->file_path);
	if (req->content)
		free(req->content);
	free(req);

	return disconnected;
}

int open_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path, 
						request_code_t mode) {
	if (storage == NULL || master_fd < 0 || client_fd < 0 || file_path == NULL || strlen(file_path) == 0 ||
		(mode != OPEN_NO_FLAGS && mode != OPEN_CREATE && mode != OPEN_LOCK && mode != OPEN_CREATE_LOCK))
		return -1;
	
	int r;

	file_t* file = NULL;

	// controllo se la modalità di apertura del file prevede la creazione del file
	if (mode == OPEN_CREATE || mode == OPEN_CREATE_LOCK) {
		NEQ0_DO(pthread_mutex_lock(&storage->mutex), r, EXTF);
		
		// recupero il file
		EQM1_DO(conc_hasht_lock(storage->files_ht, file_path), r, EXTF);
		ERRNOSET_DO(conc_hasht_get_value(storage->files_ht, file_path), file, EXTF);

		// controllo se il file esiste
		if (file != NULL) {
			EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
			NEQ0_DO(pthread_mutex_unlock(&storage->mutex), r, EXTF);
			LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
				worker_id, req_code_to_str(mode), resp_code_to_str(FILE_ALREADY_EXISTS), client_fd, file_path, 0));
			/* rispondo al cliente che il file già esiste e comunico al master di aver servito il cliente
			   (in caso di errore chiudo la connessione del cliente) */
			if (send_response_code(client_fd, FILE_ALREADY_EXISTS) == -1)
				close_client_connection(storage, master_fd, client_fd, worker_id);
			else
				EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
			free(file_path);
			return 0;
		}

		if (storage->curr_file_num == storage->max_files) {
			// TODO: espulsione del file
		}

		// creo un file e lo aggiungo allo storage
		EQNULL_DO(init_file(file_path), file, EXTF);
		EQM1_DO(conc_hasht_insert(storage->files_ht, file_path, file), r, EXTF);
		EQM1_DO(list_tail_insert(storage->files_queue, file), r, EXTF);
		
		if (mode == OPEN_CREATE_LOCK)
			file->can_write_fd = client_fd;

		(storage->curr_file_num) ++;

		if (storage->curr_file_num > storage->max_files_stored)
			storage->max_files_stored = storage->curr_file_num;
		
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d,%zu", 
			worker_id, req_code_to_str(mode), resp_code_to_str(OK), client_fd, file_path, 0, storage->curr_file_num));

		NEQ0_DO(pthread_mutex_unlock(&storage->mutex), r, EXTF);
	}
	else { // non è stata richiesta l'opzione CREATE

		// recupero il file
		EQM1_DO(conc_hasht_lock(storage->files_ht, file_path), r, EXTF);
		ERRNOSET_DO(conc_hasht_get_value(storage->files_ht, file_path), file, EXTF);

		if (file == NULL) {
			// il non file esiste
			EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
			LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d", 
				worker_id, req_code_to_str(mode), resp_code_to_str(FILE_NOT_EXISTS), client_fd, file_path, 0));
			/* rispondo al cliente che il file non esiste e comunico al master di aver servito il cliente
			   (in caso di errore chiudo la connessione del cliente) */
			if (send_response_code(client_fd, FILE_NOT_EXISTS) == -1)
				close_client_connection(storage, master_fd, client_fd, worker_id);
			else
				EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
			free(file_path);
			return 0;
		}

		// controllo se il file è già stato aperto
		EQM1_DO(int_list_contains(file->open_by_fds, client_fd), r, EXTF);
		if (r == 1) {
			EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
			LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
				worker_id, req_code_to_str(mode), resp_code_to_str(FILE_ALREADY_OPEN), client_fd, file_path, 0));
			/* rispondo al cliente che il file è già stato aperto e comunico al master di aver servito il cliente
			   (in caso di errore chiudo la connessione del cliente) */
			if (send_response_code(client_fd, FILE_ALREADY_OPEN) == -1)
				close_client_connection(storage, master_fd, client_fd, worker_id);
			else
				EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
			free(file_path);
			return 0;
		}
	}

	// inserisco il cliente tra coloro che hanno aperto il file
	EQM1_DO(int_list_tail_insert(file->open_by_fds, client_fd), r, EXTF);

	// recupero l'oggetto relativo al cliente richiedente
	client_t* client;
	EQM1_DO(conc_hasht_lock(storage->connected_clients, &client_fd), r, EXTF);
	EQNULL_DO(conc_hasht_get_value(storage->connected_clients, &client_fd), client, EXTF);

	// inserisco il file tra quelli aperti dal cliente
	EQM1_DO(list_tail_insert(client->opened_files, file), r, EXTF);

	// controllo se la modalità di apertura del file prevede la lock
	if (mode == OPEN_LOCK || mode == OPEN_CREATE_LOCK) {
		if (file->locked_by_fd == -1) {
			// il file non è bloccato
			file->locked_by_fd = client_fd;
			EQM1_DO(list_tail_insert(client->locked_files, file), r, EXTF);
		}
		else {
			// il file è già bloccato, inserisco il cliente che ha fatto richiesta nella lista di attesa
			EQM1_DO(int_list_tail_insert(file->pending_lock_fds, client_fd), r, EXTF);
			EQM1_DO(conc_hasht_unlock(storage->connected_clients, &client_fd), r, EXTF);
			EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
			LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d", 
				worker_id, req_code_to_str(mode), CLIENT_IS_WAITING, client_fd, file_path, 0));
			free(file_path);
			return 0;
		}
	}

	EQM1_DO(conc_hasht_unlock(storage->connected_clients, &client_fd), r, EXTF);
	EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);

	if (mode == OPEN_LOCK || mode == OPEN_NO_FLAGS) { // in caso di CREATE il logging è già stato effettuato
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d", 
			worker_id, req_code_to_str(mode), resp_code_to_str(OK), client_fd, file_path, 0));
	}

	/* invio l'esito positivo al cliente e comunico al master di aver servito il cliente
	   (in caso di errore chiudo la connessione del cliente) */
	if (send_response_code(client_fd, OK) == -1)
		close_client_connection(storage, master_fd, client_fd, worker_id);
	else
		EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);

	if (mode == OPEN_NO_FLAGS || mode == OPEN_LOCK) // in caso di create file_path è riferito nello storage
		free(file_path);

	return 0;
}

int write_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path, 
						void* content, 
						size_t content_size, 
						request_code_t mode) {
	return 0;
}

int read_file_handler(storage_t* storage, 
						int master_fd,
						int client_fd,
						int worker_id,
						char* file_path) {
	if (storage == NULL || master_fd < 0 || client_fd < 0 || file_path == NULL || strlen(file_path) == 0)
		return -1;
	
	int r;

	// recupero il file
	file_t* file = NULL;
	EQM1_DO(conc_hasht_lock(storage->files_ht, file_path), r, EXTF);
	ERRNOSET_DO(conc_hasht_get_value(storage->files_ht, file_path), file, EXTF);

	// controllo se il file esiste
	if (file == NULL) {
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
			worker_id, req_code_to_str(READ), resp_code_to_str(FILE_NOT_EXISTS), client_fd, file_path, 0));
		/* rispondo al cliente che il file non esiste e comunico al master di aver servito il cliente
		   (in caso di errore chiudo la connessione del cliente) */
		if (send_response_code(client_fd, FILE_NOT_EXISTS) == -1)
			close_client_connection(storage, master_fd, client_fd, worker_id);
		else
			EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
		free(file_path);
		return 0;
	}

	// controllo se il cliente ha aperto il file
	EQM1_DO(int_list_contains(file->open_by_fds, client_fd), r, EXTF);
	if (!r) {
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
			worker_id, req_code_to_str(READ), resp_code_to_str(OPERATION_NOT_PERMITTED), client_fd, file_path, 0));
		/* rispondo al cliente che l'operazione non è consentita e comunico al master di aver servito il cliente
		   (in caso di errore chiudo la connessione del cliente) */
		if (send_response_code(client_fd, OPERATION_NOT_PERMITTED) == -1)
			close_client_connection(storage, master_fd, client_fd, worker_id);
		else
			EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
		free(file_path);
		return 0;
	}

	// controllo se il file è bloccato da un altro cliente
	if (file->locked_by_fd != -1 && file->locked_by_fd != client_fd) {
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
			worker_id, req_code_to_str(READ), resp_code_to_str(OPERATION_NOT_PERMITTED), client_fd, file_path, 0));
		/* rispondo al cliente che l'operazione non è consentita e comunico al master di aver servito il cliente
		   (in caso di errore chiudo la connessione del cliente) */
		if (send_response_code(client_fd, OPERATION_NOT_PERMITTED) == -1)
			close_client_connection(storage, master_fd, client_fd, worker_id);
		else
			EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
		free(file_path);
		return 0;
	}

	LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%zu",
		worker_id, req_code_to_str(READ), resp_code_to_str(OK), client_fd, file_path, file->content_size));

	/* invio l'esito positivo al cliente
	   (in caso di errore chiudo la connessione del cliente) */
	if (send_response_code(client_fd, OK) == -1) {
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		close_client_connection(storage, master_fd, client_fd, worker_id);
		free(file_path);
		return 0;
	}
	
	/* invio il contenuto del file al cliente
	   (in caso di errore chiudo la connessione del cliente) */
	if (send_file_content(client_fd, file->content_size, file->content) == -1) {
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		close_client_connection(storage, master_fd, client_fd, worker_id);
		free(file_path);
		return 0;
	}

	EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);

	// comunico al master di aver servito il cliente
	EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);

	free(file_path);
	return 0;
}

int readn_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						int n) {
	if (storage == NULL || master_fd < 0 || client_fd < 0)
		return -1;
	
	int r;

	NEQ0_DO(pthread_mutex_lock(&storage->mutex), r, EXTF);

	int file_to_send = n;
	if (n <= 0)
		file_to_send = storage->curr_file_num;

	list_t* files_to_read;
	size_t file_sendable = 0;
	
	// creo una lista per memorizzare i file che possono essere letti al cliente
	EQNULL_DO(list_create(cmp_file, (void (*)(void*)) destroy_file), files_to_read, EXTF);

	file_t* file;
	list_for_each(storage->files_queue, file) {
		if (file_sendable == file_to_send)
			break;
		// conto i file che possono essere inviati acquisendo la lock sulla tabella hash e li memorizzo nella lista
		EQM1_DO(conc_hasht_lock(storage->files_ht, file->path), r, EXTF);
		if (file->locked_by_fd == client_fd || file->locked_by_fd == -1) {
			file_sendable ++;
			EQM1_DO(list_tail_insert(files_to_read, file), r, EXTF);
		}
		else // rilascio la lock sui file che non posso inviare
			EQM1_DO(conc_hasht_unlock(storage->files_ht, file->path), r, EXTF);
	}

	NEQ0_DO(pthread_mutex_unlock(&storage->mutex), r, EXTF);

	// invio l'esito positivo al cliente
	int err = 0;
	if (send_response_code(client_fd, OK) == -1)
		err = 1;

	// invio al cliente il numero di file che verranno inviati
	if (!err) {
		if (send_size(client_fd, file_sendable) == -1)
			err = 1;
	}

	if (file_sendable == 0) {
		LOG(log_record(storage->logger, "%d,%s 0/0,%s,%d,,%d",
			worker_id, req_code_to_str(READN), resp_code_to_str(OK), client_fd, 0));
	}

	int file_sent = 0;
	list_for_each(files_to_read, file) {
		if (!err) {
			size_t path_size = strlen(file->path) + 1;
			// invio al cliente il nome del file
			if (send_file_name(client_fd, path_size, file->path) == -1)
				err = 1;
		}
		if (!err) {
			// invio al cliente il contenuto del file
			if (send_file_content(client_fd, file->content_size, file->content) == -1)
				err = 1;
		}
		if (!err) {
			LOG(log_record(storage->logger, 
				"%d,%s %d/%zu,%s,%d,%s,%zu",
				worker_id, 
				req_code_to_str(READN), 
				file_sent + 1, 
				file_sendable, 
				resp_code_to_str(OK), 
				client_fd, file->path, 
				file->content_size));

			file_sent ++;
		}
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file->path), r, EXTF);
	}

	/* se si è verificato un errore chiudo la connessione del cliente 
	   altrimenti comunico al master di aver servito il cliente */
	if (err)
		close_client_connection(storage, master_fd, client_fd, worker_id);
	else
		EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);

	list_destroy(files_to_read, LIST_DO_NOT_FREE_DATA);
	return 0;
}

int lock_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path) {
	if (storage == NULL || master_fd < 0 || client_fd < 0 || file_path == NULL || strlen(file_path) == 0)
		return -1;
	
	int r;

	// recupero il file
	file_t* file = NULL;
	EQM1_DO(conc_hasht_lock(storage->files_ht, file_path), r, EXTF);
	ERRNOSET_DO(conc_hasht_get_value(storage->files_ht, file_path), file, EXTF);
	
	// controllo se il file esiste
	if (file == NULL) {
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
			worker_id, req_code_to_str(LOCK), resp_code_to_str(FILE_NOT_EXISTS), client_fd, file_path, 0));
		/* rispondo al cliente che il file non esiste e comunico al master di aver servito il cliente
		   (in caso di errore chiudo la connessione del cliente) */
		if (send_response_code(client_fd, FILE_NOT_EXISTS) == -1)
			close_client_connection(storage, master_fd, client_fd, worker_id);
		else
			EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
		free(file_path);
		return 0;
	}

	// controllo se il cliente ha aperto il file
	EQM1_DO(int_list_contains(file->open_by_fds, client_fd), r, EXTF);
	if (!r) {
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
			worker_id, req_code_to_str(LOCK), resp_code_to_str(OPERATION_NOT_PERMITTED), client_fd, file_path, 0));
		/* rispondo al cliente che l'operazione non è consentita e comunico al master di aver servito il cliente
		   (in caso di errore chiudo la connessione del cliente) */
		if (send_response_code(client_fd, OPERATION_NOT_PERMITTED) == -1)
			close_client_connection(storage, master_fd, client_fd, worker_id);
		else
			EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
		free(file_path);
		return 0;
	}

	// controllo se il cliente ha già acquisito la lock
	if (file->locked_by_fd == client_fd) {
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d", 
			worker_id, req_code_to_str(LOCK), resp_code_to_str(FILE_ALREADY_LOCKED), client_fd, file_path, 0));
		/* rispondo al cliente che ha già acquisito la lock e comunico al master di aver servito il cliente
		   (in caso di errore chiudo la connessione del cliente) */
		if (send_response_code(client_fd, FILE_ALREADY_LOCKED) == -1)
			close_client_connection(storage, master_fd, client_fd, worker_id);
		else
			EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
		free(file_path);
		return 0;
	}

	// controllo se il file è bloccato da un altro cliente
	if (file->locked_by_fd != -1) {
		// inserisco il cliente che ne ha fatto richiesta nella lista di attesa
		EQM1_DO(int_list_tail_insert(file->pending_lock_fds, client_fd), r, EXTF);
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
			worker_id, req_code_to_str(LOCK), CLIENT_IS_WAITING, client_fd, file_path, 0));
		free(file_path);
		return 0;
	}

	file->locked_by_fd = client_fd;

	// recupero l'oggetto relativo al cliente richiedente
	client_t* client;
	EQM1_DO(conc_hasht_lock(storage->connected_clients, &client_fd), r, EXTF);
	EQNULL_DO(conc_hasht_get_value(storage->connected_clients, &client_fd), client, EXTF);
	// inserisco il file nella lista di file bloccati dal cliente
	EQM1_DO(list_tail_insert(client->locked_files, file), r, EXTF);
	EQM1_DO(conc_hasht_unlock(storage->connected_clients, &client_fd), r, EXTF);

	EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);

	LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
		worker_id, req_code_to_str(LOCK), resp_code_to_str(OK), client_fd, file_path, 0));

	/* invio l'esito positivo al cliente e comunico al master di aver servito il cliente 
	   (in caso di errore chiudo la connessione del cliente) */
	if (send_response_code(client_fd, OK) == -1)
		close_client_connection(storage, master_fd, client_fd, worker_id);
	else
		EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
	
	free(file_path);
	return 0;
}

int unlock_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path) {
	if (storage == NULL || master_fd < 0 || client_fd < 0 || file_path == NULL || strlen(file_path) == 0)
		return -1;
	
	int r;
	
	// recupero il file
	file_t* file = NULL;
	EQM1_DO(conc_hasht_lock(storage->files_ht, file_path), r, EXTF);
	ERRNOSET_DO(conc_hasht_get_value(storage->files_ht, file_path), file, EXTF);
	
	// controllo se il file esiste
	if (file == NULL) {
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
			worker_id, req_code_to_str(UNLOCK), resp_code_to_str(FILE_NOT_EXISTS), client_fd, file_path, 0));
		/* rispondo al cliente che il file non esiste e comunico al master di aver servito il cliente
		   (in caso di errore chiudo la connessione del cliente) */
		if (send_response_code(client_fd, FILE_NOT_EXISTS) == -1)
			close_client_connection(storage, master_fd, client_fd, worker_id);
		else
			EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
		free(file_path);
		return 0;
	}

	// controllo se il cliente ha acquisito la lock sul file
	if (file->locked_by_fd != client_fd) {
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
			worker_id, req_code_to_str(UNLOCK), resp_code_to_str(OPERATION_NOT_PERMITTED), client_fd, file_path, 0));
		/* rispondo al cliente che l'operazione non è consentita e comunico al master di aver servito il cliente
		   (in caso di errore chiudo la connessione del cliente) */
		if (send_response_code(client_fd, OPERATION_NOT_PERMITTED) == -1)
			close_client_connection(storage, master_fd, client_fd, worker_id);
		else
			EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
		free(file_path);
		return 0;
	}

	// recupero l'oggetto relativo al cliente richiedente
	client_t* client;
	file_t* not_used;
	EQM1_DO(conc_hasht_lock(storage->connected_clients, &client_fd), r, EXTF);
	EQNULL_DO(conc_hasht_get_value(storage->connected_clients, &client_fd), client, EXTF);
	// rimuovo il file dalla lista di file bloccati dal cliente
	EQNULL_DO(list_remove_and_get(client->locked_files, file), not_used, EXTF);
	EQM1_DO(conc_hasht_unlock(storage->connected_clients, &client_fd), r, EXTF);

	LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d",
		worker_id, req_code_to_str(UNLOCK), resp_code_to_str(OK), client_fd, file_path, 0));

	// passo la lock sul file a un eventuale cliente in attesa
	int fd = give_lock_to_waiting_client(storage, file, worker_id, master_fd);

	// elimino la possibilità del cliente di effettuare una write sul file
	if (file->can_write_fd == client_fd)
		file->can_write_fd = -1;

	EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
	
	/* invio l'esito positivo al cliente e comunico al master di aver servito il cliente 
	   (in caso di errore chiudo la connessione del cliente) */
	if (send_response_code(client_fd, OK) == -1)
		close_client_connection(storage, master_fd, client_fd, worker_id);
	else
		EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);

	// se nel contattare il cliente in attesa della lock ho riscontrato che si è disconnesso chiudo la connessione
	if (fd != -1)
		close_client_connection(storage, master_fd, fd, worker_id);

	free(file_path);
	return 0;
}

int remove_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path) {
	return 0;
}

int close_file_handler(storage_t* storage, 
						int master_fd, 
						int client_fd, 
						int worker_id, 
						char* file_path) {
	if (storage == NULL || master_fd < 0 || client_fd < 0 || file_path == NULL || strlen(file_path) == 0)
		return -1;
	
	int r;

	// recupero il file
	file_t* file = NULL;
	EQM1_DO(conc_hasht_lock(storage->files_ht, file_path), r, EXTF);
	ERRNOSET_DO(conc_hasht_get_value(storage->files_ht, file_path), file, EXTF);
	
	// controllo se il file esiste
	if (file == NULL) {
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d", 
			worker_id, req_code_to_str(CLOSE), resp_code_to_str(FILE_NOT_EXISTS), client_fd, file_path, 0));
		/* rispondo al cliente che il file non esiste e comunico al master di aver servito il cliente
		   (in caso di errore chiudo la connessione del cliente) */
		if (send_response_code(client_fd, FILE_NOT_EXISTS) == -1)
			close_client_connection(storage, master_fd, client_fd, worker_id);
		else
			EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
		free(file_path);
		return 0;
	}

	// rimuovo il descrittore del cliente dalla lista di descrittori dei clienti che hanno aperto il file
	ERRNOSET_DO(int_list_remove(file->open_by_fds, client_fd), r, EXTF);
	if (r == -1) {
		// il cliente non aveva aperto il file
		EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);
		LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d", 
			worker_id, req_code_to_str(CLOSE), resp_code_to_str(OPERATION_NOT_PERMITTED), client_fd, file_path, 0));
		/* rispondo al cliente che l'operazione non è consentita e comunico al master di aver servito il cliente
		   (in caso di errore chiudo la connessione del cliente) */
		if (send_response_code(client_fd, OPERATION_NOT_PERMITTED) == -1)
			close_client_connection(storage, master_fd, client_fd, worker_id);
		else
			EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);
		free(file_path);
		return 0;
	}

	// recupero l'oggetto relativo al cliente richiedente
	client_t* client;
	file_t* not_used;
	EQM1_DO(conc_hasht_lock(storage->connected_clients, &client_fd), r, EXTF);
	EQNULL_DO(conc_hasht_get_value(storage->connected_clients, &client_fd), client, EXTF);

	// rimuovo il file dalla lista di file aperti dal cliente
	EQNULL_DO(list_remove_and_get(client->opened_files, file), not_used, EXTF);

	// controllo se il cliente ha acquisito la lock sul file
	if (file->locked_by_fd == client_fd) {
		// rimuovo il file dalla lista di file bloccati dal cliente
		EQNULL_DO(list_remove_and_get(client->locked_files, file), not_used, EXTF);
	}
	
	EQM1_DO(conc_hasht_unlock(storage->connected_clients, &client_fd), r, EXTF);

	int fd = -1;
	if (file->locked_by_fd == client_fd) {
		// passo la lock sul file a un eventuale cliente in attesa
		fd = give_lock_to_waiting_client(storage, file, worker_id, master_fd);
	}

	LOG(log_record(storage->logger, "%d,%s,%s,%d,%s,%d", 
		worker_id, req_code_to_str(CLOSE), resp_code_to_str(OK), client_fd, file_path, 0));

	// elimino la possibilità del cliente di effettuare una write sul file
	if (file->can_write_fd == client_fd)
		file->can_write_fd = -1;

	EQM1_DO(conc_hasht_unlock(storage->files_ht, file_path), r, EXTF);

	/* invio l'esito positivo al cliente e comunico al master di aver servito il cliente 
	   (in caso di errore chiudo la connessione del cliente) */
	if (send_response_code(client_fd, OK) == -1)
		close_client_connection(storage, master_fd, client_fd, worker_id);
	else
		EQM1_DO(writen(master_fd, &client_fd, sizeof(int)), r, EXTF);

	// se nel contattare il cliente in attesa della lock ho notato che si è disconnesso chiudo la connessione
	if (fd != -1)
		close_client_connection(storage, master_fd, fd, worker_id);

	free(file_path);
	return 0;
}

int print_statistics(storage_t* storage) {
	if (storage == NULL)
		return -1;

	int r;

	NEQ0_DO(pthread_mutex_lock(&storage->mutex), r, EXTF);

	fprintf(stdout, "================== STATISTICHE ==================\n");
	fprintf(stdout, "Massimo numero di MB memorizzati: %.6f (%zu bytes)\n", 
	(double) storage->max_bytes_stored / BYTES_IN_A_MEGABYTE, storage->max_bytes_stored);
	fprintf(stdout, "Massimo numero di file memorizzati: %zu\n", storage->max_files_stored);
	fprintf(stdout, "Numero di esecuzioni dell'algoritmo di rimpiazzamento: %zu\n", storage->evicted_files);

	if (storage->files_queue == NULL) {
		NEQ0_DO(pthread_mutex_unlock(&storage->mutex), r, EXTF);
		return 0;
	}

	file_t* file;
	if (list_is_empty(storage->files_queue)) {
		fprintf(stdout, "Nessun file attualmente memorizzato\n");
	}
	else {
		fprintf(stdout, "File attualmente memorizzati:\n");
		list_for_each(storage->files_queue, file) {
			fprintf(stdout, "%s\n", file->path);
		}
	}

	NEQ0_DO(pthread_mutex_unlock(&storage->mutex), r, EXTF);

	return 0;
}