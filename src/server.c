#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <config_parser.h>
#include <eviction_policy.h>
#include <protocol.h>
#include <util.h>
#include <threadpool.h>
#include <logger.h>
#include <log_format.h>
#include <storage_server.h>

/**
 * Numero massimo di connessioni in sospeso nella coda di ascolto del socket
 */
#if !defined(MAXBACKLOG)
#define MAXBACKLOG 64
#endif

/**
 * Massima dimensione del path del socket file
 */
#define UNIX_PATH_MAX 108

/**
 * @struct           task_args_t
 * @brief            Struttura che raccoglie gli argomenti di un task che un worker dovrà servire
 * 
 * @var storage      Struttura storage
 * @var master_fd    Descrittore per la comunicazione con il master
 * @var client_fd    Descrittore del client che ha effettuato la richiesta
 */
typedef struct task_args {
	storage_t* storage;
	int master_fd;
	int client_fd;
} task_args_t;

/**
 * @function           task_handler()
 * @brief              Funzione eseguita dai worker thread per servire le richieste dei clienti
 * 
 * @param arg          Argomenti del task
 * @param worker_id    Identificativo del worker thread che gestisce la richiesta
 */
static void task_handler(void *arg, int worker_id) {
	task_args_t* task_arg = (task_args_t*)arg;

	storage_t* storage = task_arg->storage;
	int master_fd = task_arg->master_fd;
	int client_fd = task_arg->client_fd;

	// leggo la richiesta del cliente
	request_t* req = read_request(storage, master_fd, client_fd, worker_id);
	if (req == NULL) {
		free(arg);
		return;
	}

	int r;

	// servo la richiesta
	switch (req->code) {
		case OPEN_NO_FLAGS:
		case OPEN_CREATE:
		case OPEN_LOCK:
		case OPEN_CREATE_LOCK:
			EQM1_DO(open_file_handler(
						storage, 
						master_fd, 
						client_fd, 
						worker_id, 
						req->file_path, 
						req->code),
					r, EXTF);
			break;
		case WRITE:
		case APPEND:
			EQM1_DO(write_file_handler(
						storage, 
						master_fd, 
						client_fd, 
						worker_id, 
						req->file_path, 
						req->content, 
						req->content_size, 
						req->code),
					r, EXTF);
			break;
		case READ:
			EQM1_DO(read_file_handler(
						storage,
						master_fd,
						client_fd,
						worker_id, 
						req->file_path),
					r, EXTF);
			break;
		case READN:
			EQM1_DO(readn_file_handler(
						storage,
						master_fd,
						client_fd,
						worker_id,
						req->n),
					r, EXTF);
			break;
		case LOCK:
			EQM1_DO(lock_file_handler(
						storage,
						master_fd,
						client_fd,
						worker_id,
						req->file_path),
					r, EXTF);
			break;
		case UNLOCK:
			EQM1_DO(unlock_file_handler(
						storage,
						master_fd,
						client_fd,
						worker_id,
						req->file_path),
					r, EXTF);
			break;
		case REMOVE:
			EQM1_DO(remove_file_handler(
						storage,
						master_fd,
						client_fd,
						worker_id,
						req->file_path),
					r, EXTF);
			break;
		case CLOSE:
			EQM1_DO(close_file_handler(
						storage,
						master_fd,
						client_fd,
						worker_id,
						req->file_path),
					r, EXTF);
			break;
		default: ;
	}
	free(arg);
	free(req);
}

/**
 * @struct              sighandler_args_t
 * @brief               Struttura contenente le informazioni da passare al signal handler thread
 *
 * @var set             Insieme dei segnali da gestire
 * @var signal_fd       Descrittore per la comunicazione dei segnali al main thread
 * @var shut_down       Flag da settare a seguito di ricezione di SIGHUP
 * @var shut_down_now   Flag da settare a seguito di ricezione di SIGINT o SIGQUIT
 * @var sig_mutex       Mutex per l'accesso in mutua esclusione ai flag
 */
typedef struct sighandler_args {
	sigset_t *set;
	int signal_fd;
	bool* shut_down;
	bool* shut_down_now;
	pthread_mutex_t* sig_mutex;
} sighandler_args_t;

/**
 * @function     sig_handler()
 * @breif        Funzione eseguita dal signal handler thread
 * 
 * @param arg    Argomenti della funzione
 */
static void *sig_handler(void *arg) {
	int r;
	sigset_t *set = ((sighandler_args_t*)arg)->set;
	int signal_fd   = ((sighandler_args_t*)arg)->signal_fd;
	bool* shut_down = ((sighandler_args_t*)arg)->shut_down;
	bool* shut_down_now = ((sighandler_args_t*)arg)->shut_down_now;
	pthread_mutex_t* mutex = ((sighandler_args_t*)arg)->sig_mutex;

	// maschero il segnale SIGUSR1 (i segnali in set sono già mascherati)
	EQM1_DO(sigaddset(set, SIGUSR1), r, return NULL); 
	NEQ0_DO(pthread_sigmask(SIG_BLOCK, set, NULL), r, return NULL);

	int sig;
	NEQ0_DO(sigwait(set, &sig), r, return NULL);

	switch (sig) {
		case SIGHUP:
			NEQ0_DO(pthread_mutex_lock(mutex), r, EXTF);
			*shut_down = true;
			NEQ0_DO(pthread_mutex_unlock(mutex), r, EXTF);
			EQM1(close(signal_fd), r);
			return NULL;
		case SIGINT:
		case SIGQUIT:
			NEQ0_DO(pthread_mutex_lock(mutex), r, EXTF);
			*shut_down_now = true;
			NEQ0_DO(pthread_mutex_unlock(mutex), r, EXTF);
			EQM1(close(signal_fd), r);
			return NULL;
		case SIGUSR1:
			// inviato dal main thread per sbloccare il thread dalla sigwait e forzarne l'uscita
			return NULL;
		default: ; // non vengono ricevuti altri segnali
	}	

	return NULL;
}

/**
 * @function      is_flag_setted()
 * @brief         Permette di stabilire se flag è settato a true accedendovi in mutua esclusione con mutex
 * 
 * @param mutex   Mutex per l'accesso in mutua esclusione a flag
 * @param flag    Il flag da controllare
 * 
 * @return        true se flag è true, false altrimenti
 */
static inline bool is_flag_setted(pthread_mutex_t mutex, bool flag) {
	int r;
	bool setted;
	NEQ0_DO(pthread_mutex_lock(&mutex), r, EXTF);
	setted = flag;
	NEQ0_DO(pthread_mutex_unlock(&mutex), r, EXTF);
	return setted;
}

/**
 * @function      set_flag()
 * @brief         Setta a true flag accedendovi in mutua esclusione
 * 
 * @param mutex   Mutex per l'accesso in mutua esclusione a flag
 * @param flag    Il flag da settare
 */
static inline void set_flag(pthread_mutex_t mutex, bool* flag) {
	int r;
	NEQ0_DO(pthread_mutex_lock(&mutex), r, EXTF);
	*flag = true;
	NEQ0_DO(pthread_mutex_unlock(&mutex), r, EXTF);
}

/**
 * @function      get_max_fd()
 * @brief         Ritorna il descrittore di indice massimo tra i descrittori attivi
 * 
 * @param set     Set dei descrittori attivi
 * @param fdmax   Indice del massimo descrittore attuale
 * 
 * @return        Indice del massimo descrittore attivo
 */
static int get_max_fd(fd_set set, int fdmax) {
	for (int i = (fdmax-1); i >= 0; -- i) {
		if (FD_ISSET(i, &set)) 
			return i;
	}
	return -1;
}

/**
 * @function      usage()
 * @brief         stampa il messaggio di usage.
 * 
 * @param prog    Il nome del programma
 */
static void usage(char* prog) {
	printf("usage: prog [-h] [-c config_file_path]\n\n"
	"Se l'opzione -c non viene specificata verrà utilizzato il file di configurazione '%s'.\n"
	"Il file di configurazione deve avere il seguente formato:\n\n"
	"# Questo è un commento (linea che inizia con '#').\n", DEFAULT_CONFIG_PATH);
	printf("# Le linee che presentano solo caratteri di spaziatura verrano anch'esse ignorate.\n");
	printf("# Le linee possono essere al più lunghe %d caratteri.\n", CONFIG_LINE_SIZE);
	printf("# Una linea può contenere una coppia chiave-valore, separati da '=' e terminante con ';'.\n");
	printf("# Sono ammessi caratteri di spaziatura solo dopo ';'.\n");
	printf("# Una chiave può essere specificata una sola volta.\n");
	printf("# Se una chiave non viene specificata verranno utilizzati i valori di default.\n\n");
	printf("# Di seguito le chiavi ammissibili (non è necessario che siano specificate in questo ordine):\n\n");
	printf("# Numero di thread workers\n");
	printf("# (n intero, n > 0, se non specificato = %u)\n", DEFAULT_N_WORKERS);
	printf("%s=n;\n\n", N_WORKERS_STR);
	printf("# Dimensione della coda di task pendenti nel thread pool\n");
	printf("# (n intero, 0 < n <= %zu, se non specificato = %lu)\n", SIZE_MAX, DEFAULT_DIM_WORKERS_QUEUE);
	printf("%s=n;\n\n", DIM_WORKERS_QUEUE_STR);
	printf("# Numero massimo di file che possono essere memorizzati nello storage\n");
	printf("# (n intero, 0 < n <= %zu, se non specificato = %u)\n", SIZE_MAX, DEFAULT_MAX_FILES);
	printf("%s=n;\n\n", MAX_FILE_NUM_STR);
	printf("# Numero massimo di bytes che possono essere memorizzati nello storage\n");
	printf("# (n intero, 0 < n <= %zu [circa %.0f MB], se non specificato = %u)\n", 
	SIZE_MAX, (double) SIZE_MAX / 1000000, DEFAULT_MAX_BYTES);
	printf("%s=n;\n\n", MAX_BYTES_STR);
	printf("# Numero massimo di lock che possono essere associate ai files\n");
	printf("# (n intero, 0 < n <= %zu, se non specificato = %u)\n", 
	SIZE_MAX, DEFAULT_MAX_LOCKS);
	printf("%s=n;\n\n", MAX_LOCKS_STR);
	printf("# Numero atteso di clienti contemporaneamente connessi\n");
	printf("# (n intero, 0 < n <= %zu, se non specificato = %u)\n", 
	SIZE_MAX, DEFAULT_EXPECTED_CLIENTS);
	printf("%s=n;\n\n", EXPECTED_CLIENTS_STR);
	printf("# Path della socket per la connessione con i clienti\n");
	printf("# (se non specificato = %s)\n", DEFAULT_SOCKET_PATH);
	printf("%s=path;\n\n", SOCKET_PATH_STR);
	printf("# Path del file di log\n");
	printf("# (ad ogni esecuzione se già esiste viene sovrascritto, se non specificato = %s)\n", DEFAULT_LOG_PATH);
	printf("%s=path;\n\n", LOG_FILE_STR);
	printf("# Politica di espulsione dei file\n");
	printf("# (policy può assumere uno tra i seguenti valori %s|%s|%s, se non specificato = %s)\n", 
	eviction_policy_to_str(FIFO),
	eviction_policy_to_str(LRU),
	eviction_policy_to_str(LFU),
	eviction_policy_to_str(DEFAULT_EVICTION_POLICY));
	printf("%s=policy;\n", EVICTION_POLICY_STR);
}

int main(int argc, char *argv[]) {
	int r, extval = EXIT_SUCCESS;

	// maschero i segnali SIGINT, SIGQUIT e SIGHUP
	sigset_t mask;
	EQM1_DO(sigemptyset(&mask), r, EXTF);
	EQM1_DO(sigaddset(&mask, SIGINT), r, EXTF); 
	EQM1_DO(sigaddset(&mask, SIGQUIT), r, EXTF);
	EQM1_DO(sigaddset(&mask, SIGHUP), r, EXTF);
	NEQ0_DO(pthread_sigmask(SIG_BLOCK, &mask, NULL), r, EXTF);

	// ignoro il segnale SIGPIPE
	struct sigaction s;
	memset(&s, '0', sizeof(struct sigaction));
	s.sa_handler = SIG_IGN;
	EQM1_DO(sigaction(SIGPIPE, &s, NULL), r, EXTF);

	// pipe per la comunicazione con il thread destinato alla ricezione dei segnali
	int signal_pipe[2];
	EQM1_DO(pipe(signal_pipe), r, EXTF);

	// flag settato al momento della ricezione di SIGHUP
	bool shut_down = false;
	// flag settato al momento della ricezione di SIGINT o SIGQUIT
	bool shut_down_now = false;

	// mutex per l'accesso in mutua esclusione ai flag shut_down e shut_down_now
	pthread_mutex_t sig_mutex;
	NEQ0_DO(pthread_mutex_init(&sig_mutex, NULL), r, EXTF);

	// creo il thread destinato alla ricezione dei segnali
	pthread_t sig_handler_thread;
	sighandler_args_t sighandler_args = { &mask, signal_pipe[1], &shut_down, &shut_down_now, &sig_mutex };
	NEQ0_DO(pthread_create(&sig_handler_thread, NULL, sig_handler, &sighandler_args), r, EXTF);

	// effettuo il parsing degli argomenti della linea di comando
	char* config_file = NULL;
	config_t* config = NULL;
	int option;
	while ((option = getopt(argc, argv, ":hc:")) != -1) {
		switch (option) {
			case 'h':
				// stampo il messaggio di help
				usage(argv[0]);
				goto server_exit;
			case 'c':
				// controllo se l'opzione è già stata specificata
				if (config_file != NULL) {
					fprintf(stderr, "ERR: l'opzione -c può essere specificata una sola volta\n");
					extval = EXIT_FAILURE;
					goto server_exit;
				}
				// controllo se l'opzione presenta un argomento
				if (optarg[0] == '-') {
					fprintf(stderr, "ERR: l'opzione -c necessita un argomento\n");
					extval = EXIT_FAILURE;
					goto server_exit;
				}
				// copio il path del file di configurazione
				size_t config_file_len = strlen(optarg) + 1;
				EQNULL_DO(calloc(config_file_len, sizeof(char)), config_file, EXTF);
				strcpy(config_file, optarg);
				break;
			case ':':
				fprintf(stderr, "ERR: l'opzione -c necessita un argomento\n");
				extval = EXIT_FAILURE;
				goto server_exit;
			case '?':
				fprintf(stderr, "ERR, opzione -'%c' non riconosciuta\n", optopt);
				extval = EXIT_FAILURE;
				goto server_exit;
		}
	}

	// effettuo il parsing del file di configurazione
	EQNULL_DO(config_init(), config, EXTF);
	if (config_parser(config, config_file) == -1) {
		extval = EXIT_FAILURE;
		goto server_exit;
	}
	free(config_file);
	config_file = NULL;

	// stampo i valori di configurazione
	printf("=========== VALORI DI CONFIGURAZIONE ===========\n");
	printf("%s = %zu\n", N_WORKERS_STR, config->n_workers);
	printf("%s = %zu\n", DIM_WORKERS_QUEUE_STR, config->dim_workers_queue);
	printf("%s = %zu\n", MAX_FILE_NUM_STR, config->max_file_num);
	printf("%s = %zu\n", MAX_BYTES_STR, config->max_bytes);
	printf("%s = %zu\n", MAX_LOCKS_STR, config->max_locks);
	printf("%s = %zu\n", EXPECTED_CLIENTS_STR, config->expected_clients);
	printf("%s = %s\n", SOCKET_PATH_STR, config->socket_path);   
	printf("%s = %s\n", LOG_FILE_STR, config->log_file_path);
	printf("%s = %s\n", EVICTION_POLICY_STR, eviction_policy_to_str(config->eviction_policy));

	// set up del welcoming socket
	int listenfd;
	EQM1_DO(socket(AF_UNIX, SOCK_STREAM, 0), listenfd, EXTF);
	struct sockaddr_un serv_addr;
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;    
	strncpy(serv_addr.sun_path, config->socket_path, strlen(config->socket_path) + 1);
	unlink(config->socket_path); // rimuovo il socket se già esistente
	EQM1_DO(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), r, extval = EXIT_FAILURE; goto server_exit);
	EQM1_DO(listen(listenfd, MAXBACKLOG), r, EXTF);
	
	// creo il threadpool
	threadpool_t *pool = NULL;
	EQNULL_DO(threadpool_create(config->n_workers, config->dim_workers_queue), pool, EXTF);

	// pipe per la comunicazione tra master e workers
	int workers_pipe[2];
	EQM1_DO(pipe(workers_pipe), r, EXTF);

	// creo l'oggetto logger
	logger_t* logger;
	EQNULL_DO(logger_create(config->log_file_path, INIT_LINE), logger, EXTF);

	// creo l'oggetto storage
	storage_t* storage = NULL;
	EQNULL_DO(create_storage(config, logger), storage, EXTF);

	// maschere per la gestione del selettore
	fd_set set, tmpset;
	FD_ZERO(&set);
	FD_ZERO(&tmpset);
	FD_SET(listenfd, &set);
	FD_SET(signal_pipe[0], &set);
	FD_SET(workers_pipe[0], &set);
	int fdmax = (listenfd > signal_pipe[0]) ? listenfd : signal_pipe[0];
	fdmax = (workers_pipe[0] > fdmax) ? workers_pipe[0] : fdmax;
	
	// numero di clienti connessi
	int connected_clients = 0;

	// main loop
	while (!is_flag_setted(sig_mutex, shut_down_now)) {
		// se shut_down è settato elimino il listenfd dalla maschera
		if (is_flag_setted(sig_mutex, shut_down)) {
			if (listenfd != -1) {
				FD_CLR(listenfd, &set);
				if (listenfd == fdmax)
					fdmax = get_max_fd(set, fdmax);
				EQM1(close(listenfd), r);
				listenfd = -1;
			}
		}
		
		tmpset = set;
		EQM1_DO(select(fdmax + 1, &tmpset, NULL, NULL, NULL), r, EXTF);
		
		for (int i = 0; i <= fdmax; i ++) {
			if (is_flag_setted(sig_mutex, shut_down_now)) {
				LOG(log_record(logger, "%d,%s", 
					MASTER_ID, SHUT_DOWN_NOW));
				break;
			}

			if (!FD_ISSET(i, &tmpset))
				continue;
			
			int client_fd;
			if (i == listenfd) {
				// è giunta una nuova richiesta di connessione
				if (is_flag_setted(sig_mutex, shut_down_now)) {
					LOG(log_record(logger, "%d,%s", 
						MASTER_ID, SHUT_DOWN_NOW));
					break;
				}
				if (is_flag_setted(sig_mutex, shut_down))
					continue;
				
				EQM1_DO(client_fd = accept(listenfd, (struct sockaddr*)NULL ,NULL), r, EXTF);
				FD_SET(client_fd, &set);
				if (client_fd > fdmax)
					fdmax = client_fd;

				EQM1_DO(new_connection_handler(storage, client_fd), r, EXTF);

				connected_clients++;

				LOG(log_record(logger, "%d,%s,,%d,,,,,%d",
					MASTER_ID, NEW_CONNECTION, client_fd, connected_clients));
			}
			else if (i == signal_pipe[0]) {
				// il thread destinato alla ricezione di segnali ha scritto nella pipe
				if (is_flag_setted(sig_mutex, shut_down_now)) {
					LOG(log_record(logger, "%d,%s", 
						MASTER_ID, SHUT_DOWN_NOW));
					break;
				}
				else {
					LOG(log_record(logger, "%d,%s", 
						MASTER_ID, SHUT_DOWN));
				}

				FD_CLR(signal_pipe[0], &set);
				if (signal_pipe[0] == fdmax)
					fdmax = get_max_fd(set, fdmax);

				// se non ci sono più clienti connessi posso terminare
				if (connected_clients == 0) {
					set_flag(sig_mutex, &shut_down_now);
					break;
				}
			}
			else if (i == workers_pipe[0]) {
				// un worker ha scritto nella pipe destinata alle comunicazioni tra master e workers

				// leggo il descrittore scritto dal worker nella pipe
				EQM1_DO(readn(workers_pipe[0], &client_fd, sizeof(int)), r, EXTF);

				// se negativo significa che il cliente associato al descrittore -(client_fd) si è disconnesso
				if (client_fd < 0) {
					connected_clients --;
					EQM1(close((-client_fd)), r);
					LOG(log_record(logger, "%d,%s,,%d,,,,,%d",
						MASTER_ID, CLOSED_CONNECTION, (-client_fd), connected_clients));
					// se è stato ricevuto il segnale SIGHUP e non ci sono più clienti connessi posso terminare
					if (is_flag_setted(sig_mutex, shut_down) && connected_clients == 0) {
						set_flag(sig_mutex, &shut_down_now);
						break;
					}
				}
				// altrimenti il cliente associato al descrittore client_fd è stato servito
				else {
					FD_SET(client_fd, &set);
					if (client_fd > fdmax)
						fdmax = client_fd;
				}
			}
			else {
				// è stata ricevuta una richiesta da un cliente già connesso

				if (is_flag_setted(sig_mutex, shut_down_now)) {
					LOG(log_record(logger, "%d,%s", 
						MASTER_ID, SHUT_DOWN_NOW));
					break;
				}
				
				client_fd = i; 
				FD_CLR(client_fd, &set); 
				if (client_fd == fdmax)
					fdmax = get_max_fd(set, fdmax);

				// inizializzo gli argomenti della funzione che sarà eseguita da un worker per servire la richiesta
				task_args_t* args = NULL;
				EQNULL_DO(malloc(sizeof(task_args_t)), args, EXTF);
				args->storage = storage;
				args->master_fd = workers_pipe[1];
				args->client_fd = client_fd;
			
				// aggiugo al threadpool la richiesta
				EQM1_DO(threadpool_add(pool, task_handler, (void*)args), r, EXTF);
				// controllo se il threadpool ha respinto il task
				if (r == 1) {
					// il threadpool ha rifiutato il task
					if (rejected_task_handler(storage, workers_pipe[1], client_fd) == 0) {
						// se il client non si è disconnesso aggiungo il suo descrittore al set
						FD_SET(client_fd, &set);
						if (client_fd > fdmax)
							fdmax = client_fd;
					}
					free(args);
				}
			}
		}
	}
	if (listenfd != -1)
		EQM1(close(listenfd), r);
	
	threadpool_destroy(pool);
	EQM1(unlink(config->socket_path), r);
	NEQ0(pthread_join(sig_handler_thread, NULL), r);
	NEQ0_DO(pthread_mutex_destroy(&sig_mutex), r, EXTF);

	// stampo le statistiche
	EQM1_DO(print_statistics(storage), r, EXTF);

	destroy_storage(storage);
	logger_destroy(logger);
	config_destroy(config);
	return 0;

server_exit:
	if (config_file)
		free(config_file);
	config_destroy(config);
	// invio un segnale al thread dedicato alla ricezione di segnali
	NEQ0(pthread_kill(sig_handler_thread, SIGUSR1), r);
	NEQ0(pthread_join(sig_handler_thread, NULL), r);
	NEQ0_DO(pthread_mutex_destroy(&sig_mutex), r, EXTF);
	return extval;
}