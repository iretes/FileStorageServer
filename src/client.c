#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include <client_api.h>
#include <cmdline_operation.h>
#include <cmdline_parser.h>
#include <list.h>
#include <filesys_util.h>
#include <util.h>

/** Secondi da dedicare ai tentativi di connessione verso il server */
#define TRY_CONN_FOR_SEC 5
/** Millisecondi da attendere tra un tentativo di connessione verso il server e il successivo */
#define RETRY_CONN_AFTER_MSEC 1000
/** Massimo numero di richieste da effettuare nel caso in cui il server risponda che è troppo occupato */
#define MAX_REQ_TRIES 3
/** Millisecondi da attendere tra una richiesta fallita perchè il server è troppo occupato e il tentativo di richiesta 
 * successivo*/
#define RETRY_REQ_AFTER_MSEC 1000

/**
 * @def          RETRY_IF_BUSY()
 * @brief        Tenta al più MAX_REQ_TRIES volte la chiamata dell'api X nel caso in cui il server sia occupato.
 * 
 * @param X      La chiamata dell'api
 * @param ret    Il valore ritornato da X
 */
#define RETRY_IF_BUSY(X, ret) \
	do { \
		int try = 0; \
		bool sleep_fail = 0; \
		while ((ret = (X)) == -1 && errno == EBUSY) { \
			try ++; \
			if (try >= MAX_REQ_TRIES) break; \
			PRINT(" : %s", errno_to_str(errno)); \
			if (millisleep(RETRY_REQ_AFTER_MSEC) == -1)  { \
				fprintf(stderr, "\nERR: millisleep (%s)\n", strerror(errno)); \
				sleep_fail = 1; \
				errno = EBUSY; \
				break; \
			} \
		} \
		if (!sleep_fail) \
			PRINT(" : %s", ret == -1 ? errno_to_str(errno) : "OK"); \
	} while(0);

/**
 * @function     should_exit()
 * @brief        Consente di stabilire in base al codice di errore err settato nella api se il processo deve terminare o può 
 *               proseguire l'esecuzione
 * 
 * @param err    Codice di errore
 * @return       True se il processo deve terminare l'esecuzione, false atrimenti
 */
static inline bool should_exit(int err) {
	if (errno == ECONNRESET || errno == ECOMM || errno == EBUSY)
		return true;
	return false;
}

/**
 * @function                   write_file_list()
 * @brief                      Effettua la richiesta di scrittura al server, invocando le funzioni dell'api, 
 *                             per ogni file della lista cmdline_operation->files.
 * 
 * @param cmdline_operation    L'operazione della linea di comando e i suoi argomenti
 * 
 * @return                     0 in caso di successo, in caso di fallimento ritorna -1 se si è verificato un errore che 
 *                             dovrà essere gestito terminando il processo, 1 se si è verificato un errore ma è possibile 
 *                             effettuare le eventuali operazioni successive.
 */

static int write_file_list(cmdline_operation_t* cmdline_operation) {
	if (!cmdline_operation || !cmdline_operation->files) {
		fprintf(stderr, "\nERR: argomenti non validi nella funzione '%s'\n", __func__);
		return 1;
	}
	int ret, errnosv;
	char* filepath;
	// itero sui file che devono essere scritti
	list_for_each(cmdline_operation->files, filepath) {
		// ottengo il path assoluto del file
		char* abspath = get_absolute_path(filepath);
		if (!abspath) {
			fprintf(stderr, "\nERR: get_absolute_path di '%s' (%s)\n", 
				filepath, strerror(errno));
			if (errno == ENOMEM) return -1;
			continue;
		}

		// invoco la funzione dell'api per aprire il file
		PRINT("\nopenFile(pathname = %s, flags = O_CREATE|O_LOCK)", abspath);
		RETRY_IF_BUSY(openFile(abspath, O_CREATE|O_LOCK), ret);
		if (ret == -1) { 
			errnosv = errno;
			free(abspath);
			if (errnosv == EBADRQC) return 1;
			else if (should_exit(errnosv)) return -1;
			else continue;
		}

		// invoco la funzione dell'api per scrivere il file 
		PRINT("\nwriteFile(pathname = %s)", abspath);
		RETRY_IF_BUSY(writeFile(abspath, cmdline_operation->dirname_out), ret);
		if (ret == -1 && errno != EFAULT) {
			errnosv = errno;
			free(abspath);
			if (errnosv == EBADRQC) return 1;
			else if (should_exit(errnosv)) return -1;
			else continue;
		}

		// invoco la funzione dell'api per chiudere il file
		PRINT("\ncloseFile(pathname = %s)", abspath);
		RETRY_IF_BUSY(closeFile(abspath), ret);
		if (ret == -1) {
			errnosv = errno;
			free(abspath);
			if (errnosv == EBADRQC) return 1;
			else if (should_exit(errnosv)) return -1;
			else continue;
		}
		free(abspath);
	}
	return 0;
}

/**
 * @function                   read_file_list()
 * @brief                      Esegue l'operazione 'r'.
 * 
 * @param cmdline_operation    L'operazione della linea di comando e i suoi argomenti
 * 
 * @return                     0 in caso di successo, in caso di fallimento ritorna -1 se si è verificato un errore che 
 *                             dovrà essere gestito terminando il processo, 1 se si è verificato un errore ma è possibile 
 *                             effettuare le eventuali operazioni successive.
 */
static int read_file_list(cmdline_operation_t* cmdline_operation) {
	if (!cmdline_operation || !cmdline_operation->files) {
		fprintf(stderr, "\nERR: argomenti non validi nella funzione '%s'\n", __func__);
		return 1;
	}

	if (cmdline_operation->dirname_out) {
		// creo, se non esiste, la directory in cui memorizzare i file letti dal server
		if (mkdirr(cmdline_operation->dirname_out) == -1) {
			fprintf(stderr, "\nERR: mkdirr di '%s' (%s), i file ricevuti non saranno scritti su disco\n",
				cmdline_operation->dirname_out, strerror(errno));
			// se non è possibile creare la directory e l'errore è diverso da ENOMEM proseguo senza salvare i file
			cmdline_operation->dirname_out = NULL;
			if (errno == ENOMEM) return -1;
		}
	}

	char* filepath;
	// itero sui file da leggere
	list_for_each(cmdline_operation->files, filepath) {
		// ottengo il path assoluto del file
		char* abspath = get_absolute_path(filepath);
		if (!abspath) {
			fprintf(stderr, "\nERR: get_absolute_path di '%s' (%s)\n",
				filepath, strerror(errno));
			if (errno == ENOMEM) return -1;
			continue;
		}

		// invoco la funzione dell'api per aprire il file
		int ret, errnosv;
		PRINT("\nopenFile(pathname = %s, flags = 0)", abspath);
		RETRY_IF_BUSY(openFile(abspath, 0), ret);
		if (ret == -1 && errno != EALREADY) { 
			errnosv = errno;
			free(abspath);
			if (errnosv == EBADRQC) return 1;
			else if (should_exit(errnosv)) return -1;
			else continue;
		}

		// invoco la funzione dell'api per leggere il file
		void* buf = NULL;
		size_t size = 0;
		PRINT("\nreadFile(pathname = %s)", abspath);
		RETRY_IF_BUSY(readFile(abspath, &buf, &size), ret);
		if (ret == -1) { 
			errnosv = errno;
			free(abspath);
			if (errnosv == EBADRQC) return 1;
			else if (should_exit(errnosv)) return -1;
			else continue;
		}

		if (cmdline_operation->dirname_out && size != 0) {
			// ottengo il nome del file letto
			char* filename = get_basename(abspath);
			if (!filename) {
				if (errno == ENOMEM) return -1;
				else continue;
			}
			// costruisco il path del file letto per poterlo memorizzare nella directory cmdline_operation->dirname_out
			char* new_filepath = build_notexisting_path(cmdline_operation->dirname_out, filename);
			if (!new_filepath) {
				if (errno == ENOMEM) return -1;
				else continue;
			}
			if (new_filepath) {
				// creo il file
				FILE * file = fopen(new_filepath, "w+");
				if (file) {
					// scrivo nel file
					if (fwrite(buf, 1, size, file) != size) {
						fprintf(stderr, "\nERR: fwrite '%s' (%s)\n",
							new_filepath, strerror(errno));
					}
					// chiudo il file
					if (fclose(file) == -1) {
						fprintf(stderr, "\nERR: fclose '%s' (%s)\n",
							new_filepath, strerror(errno));
					}
					PRINT(" (%zu bytes salvati in %s)", size, new_filepath);
				}
				else {
					fprintf(stderr, "\nERR: fopen di '%s' con modalità w+ (%s)\n",
						new_filepath, strerror(errno));
				}
				free(new_filepath);
			}
			else {
				fprintf(stderr, "\nERR: build_notexisting_path per scrivere il file '%s' in '%s' (%s)\n",
					filename, cmdline_operation->dirname_out, strerror(errno));
			}
			free(filename);
		}
		if (buf) 
			free(buf);
		if (errno == ENOMEM)
			return -1;

		// invoco la funzione dell'api per chiudere il file
		PRINT("\ncloseFile(pathname = %s)", abspath);
		RETRY_IF_BUSY(closeFile(abspath), ret);
		if (ret == -1) {
			errnosv = errno;
			free(abspath);
			if (errnosv == EBADRQC) return 1;
			else if (should_exit(errnosv)) return -1;
			else continue;
		}
		free(abspath);
	}
	return 0;
}

/**
 * @function                   lock_file_list()
 * @brief                      Esegue l'operazione 'l'.
 * 
 * @param cmdline_operation    L'operazione della linea di comando e i suoi argomenti
 * 
 * @return                     0 in caso di successo, in caso di fallimento ritorna -1 se si è verificato un errore che 
 *                             dovrà essere gestito terminando il processo, 1 se si è verificato un errore ma è possibile 
 *                             effettuare le eventuali operazioni successive.
 */
int lock_file_list(cmdline_operation_t* cmdline_operation) {
	if (!cmdline_operation || !cmdline_operation->files) {
		fprintf(stderr, "\nERR: argomenti non validi nella funzione '%s'\n", __func__);
		return 1;
	}

	char* filepath;
	// itero sui file su cui acquisire la lock
	list_for_each(cmdline_operation->files, filepath) {
		// ottengo il path assoluto del file
		char* abspath = get_absolute_path(filepath);
		if (!abspath) {
			fprintf(stderr, "\nERR: get_absolute_path di '%s' (%s)\n", 
				filepath, strerror(errno));
			if (errno == ENOMEM) return -1;
			continue;
		}

		// invoco la funzione dell'api per aprire il file
		int ret, errnosv;
		PRINT("\nopenFile(pathname = %s, flags = 0)", abspath);
		RETRY_IF_BUSY(openFile(abspath, 0), ret);
		if (ret == -1 && errno != EALREADY) { 
			errnosv = errno;
			free(abspath);
			if (errnosv == EBADRQC) return 1;
			else if (should_exit(errnosv)) return -1;
			else continue;
		}

		// invoco la funzione dell'api per acquisire la lock sul file
		PRINT("\nlockFile(pathname = %s)", abspath);
		RETRY_IF_BUSY(lockFile(abspath), ret);
		if (ret == -1) { 
			errnosv = errno;
			free(abspath);
			if (errnosv == EBADRQC) return 1;
			else if (should_exit(errnosv)) return -1;
			else continue;
		}
		free(abspath);
	}
	return 0;
}

/**
 * @function                   unlock_file_list()
 * @brief                      Esegue l'operazione 'u'.
 * 
 * @param cmdline_operation    L'operazione della linea di comando e i suoi argomenti
 * 
 * @return                     0 in caso di successo, in caso di fallimento ritorna -1 se si è verificato un errore che 
 *                             dovrà essere gestito terminando il processo, 1 se si è verificato un errore ma è possibile 
 *                             effettuare le eventuali operazioni successive.
 */
int unlock_file_list(cmdline_operation_t* cmdline_operation) {
	if (!cmdline_operation || !cmdline_operation->files) {
		fprintf(stderr, "\nERR: argomenti non validi nella funzione '%s'\n", __func__);
		return 1;
	}
	
	char* filepath;
	// itero sui file su cui rilasciare la lock
	list_for_each(cmdline_operation->files, filepath) {
		// ottengo il path assoluto del file
		char* abspath = get_absolute_path(filepath);
		if (!abspath) {
			fprintf(stderr, "\nERR: get_absolute_path di '%s' (%s)\n", 
				filepath, strerror(errno));
			if (errno == ENOMEM) return -1;
			continue;
		}

		// invoco la funzione dell'api per rilasciare la lock sul file
		int ret, errnosv;
		PRINT("\nunlockFile(pathname = %s)", abspath);
		RETRY_IF_BUSY(unlockFile(abspath), ret);
		if (ret == -1) { 
			errnosv = errno;
			free(abspath);
			if (errnosv == EBADRQC) return 1;
			else if (should_exit(errnosv)) return -1;
			else continue;
		}
		free(abspath);
	}
	return 0;
}

/**
 * @function                   remove_file_list()
 * @brief                      Esegue l'operazione 'c'.
 * 
 * @param cmdline_operation    L'operazione della linea di comando e i suoi argomenti
 * 
 * @return                     0 in caso di successo, in caso di fallimento ritorna -1 se si è verificato un errore che 
 *                             dovrà essere gestito terminando il processo, 1 se si è verificato un errore ma è possibile 
 *                             effettuare le eventuali operazioni successive.
 */
int remove_file_list(cmdline_operation_t* cmdline_operation) {
	if (!cmdline_operation || !cmdline_operation->files) {
		fprintf(stderr, "\nERR: argomenti non validi nella funzione '%s'\n", __func__);
		return 1;
	}
	
	char* filepath;
	// itero sui file da rimuovere
	list_for_each(cmdline_operation->files, filepath) {
		// ottengo il path assoluto del file
		char* abspath = get_absolute_path(filepath);
		if (!abspath) {
			fprintf(stderr, "\nERR: get_absolute_path di '%s' (%s)\n", 
				filepath, strerror(errno));
			if (errno == ENOMEM) return -1;
			continue;
		}

		// invoco la funzione dell'api per aprire il file
		int ret, errnosv;
		PRINT("\nopenFile(pathname = %s, flags = O_LOCK)", abspath);
		RETRY_IF_BUSY(openFile(abspath, O_LOCK), ret);
		// se il file è già aperto invoco la funzione dell'api per acquisire la lock sul file
		if (ret == -1 && errno == EALREADY) {
			PRINT("\nlockFile(pathname = %s)", abspath);
			RETRY_IF_BUSY(lockFile(abspath), ret);
			if (ret == -1) { 
				errnosv = errno;
				free(abspath);
				if (errnosv == EBADRQC) return 1;
				else if (should_exit(errnosv)) return -1;
				else continue;
			}
		}
		else if (ret == -1) { 
			errnosv = errno;
			free(abspath);
			if (errnosv == EBADRQC) return 1;
			else if (should_exit(errnosv)) return -1;
			else continue;
		}

		// invoco la funzione dell'api per rimuovere il file
		PRINT("\nremoveFile(pathname = %s)", abspath);
		RETRY_IF_BUSY(removeFile(abspath), ret);
		if (ret == -1) { 
			errnosv = errno;
			free(abspath);
			if (errnosv == EBADRQC) return 1;
			else if (should_exit(errnosv)) return -1;
			else continue;
		}
		free(abspath);
	}
	return 0;
}

/**
 * @function                   read_n_files()
 * @brief                      Esegue l'operazione 'R'.
 * 
 * @param cmdline_operation    L'operazione della linea di comando e i suoi argomenti
 * 
 * @return                     0 in caso di successo, in caso di fallimento ritorna -1 se si è verificato un errore che 
 *                             dovrà essere gestito terminando il processo, 1 se si è verificato un errore ma è possibile 
 *                             effettuare le eventuali operazioni successive.
 */
int read_n_files(cmdline_operation_t* cmdline_operation) {
	if (!cmdline_operation) {
		fprintf(stderr, "\nERR: argomenti non validi nella funzione '%s'\n", __func__);
		return 1;
	}
	if (cmdline_operation->dirname_out) {
		// creo, se non esiste, la directory in cui memorizzare i file letti dal server
		if (mkdirr(cmdline_operation->dirname_out) == -1) {
			fprintf(stderr, "\nERR: mkdirr di '%s' (%s), i file ricevuti non saranno scritti su disco\n", 
				cmdline_operation->dirname_out, strerror(errno));
			cmdline_operation->dirname_out = NULL;
			if (errno == ENOMEM) return -1;
		}
	}

	// invoco la funzione dell'api per effettuare la readn
	int ret;
	PRINT("\nreadNFiles(N = %d)", cmdline_operation->n);
	RETRY_IF_BUSY(readNFiles(cmdline_operation->n, cmdline_operation->dirname_out), ret);
	if (ret == -1) { 
		if (should_exit(errno)) return -1;
		else return 1;
	}
	PRINT(" (%d file ricevuti)", ret);
	return 0;
}

int main(int argc, char* argv[]) {
	int r;
	int extval = EXIT_SUCCESS;

	if (argc == 1) {
		fprintf(stderr, "ERR: %s necessita almeno un argomento, usa -h per maggiori informazioni\n", argv[0]);
		extval = EXIT_FAILURE;
		goto exit;
	}

	// effettuo il parsing degli argomenti della linea di comando
	char* sockname = NULL;
	list_t* cmdline_operation_list = NULL;
	cmdline_operation_list = cmdline_parser(argc, argv, &sockname);
	if (!cmdline_operation_list) {
		if (errno != 0)
			extval = EXIT_FAILURE;
		else 
			extval =  EXIT_SUCCESS;
		goto exit;
	}
	if (list_is_empty(cmdline_operation_list)) {
		fprintf(stderr, "ERR: %s non ha argomenti, usa -h per maggiori informazioni\n", argv[0]);
		extval = EXIT_FAILURE;
		goto exit;
	}

	cmdline_operation_t* cmdline_operation;
	// se le stampe sono abilitate stampo sullo stdout le operazioni richieste
	if (is_printing_enable()) {
		printf("============= OPERAZIONI RICHIESTE =============\n");
		printf("-f %s\n", sockname);
		printf("-p\n");
		list_for_each(cmdline_operation_list, cmdline_operation) {
			cmdline_operation_print(cmdline_operation);
		}
		printf("================================================\n");
	}

	// maschero SIGPIPE
	struct sigaction s;
	memset(&s, '0', sizeof(struct sigaction));
	s.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &s, NULL) == -1) {
		fprintf(stderr, "ERR: sigaction (%s)\n", strerror(errno));
		extval = EXIT_FAILURE;
		goto exit;
	}
	
	// ottengo il tempo corrente e calcolo il tempo assoluto necessario per la funzione della api openConnection()
	time_t curr_time = time(NULL);
	if (curr_time == -1) {
		fprintf(stderr, "ERR: time (%s)\n", strerror(errno));
		extval = EXIT_FAILURE;
		goto exit;
	}
	struct timespec abstime;
	abstime.tv_nsec = 0;
	abstime.tv_sec = curr_time + TRY_CONN_FOR_SEC;

	enable_printing();

	// invoco la funzione della api per l'apertura della connessione con il server
	r = openConnection(sockname, RETRY_CONN_AFTER_MSEC, abstime);
	PRINT("openConnection(sockname = %s) : %s",
		sockname, r == -1 ? errno_to_str(errno) : "OK");
	if (r == -1) {
		PRINT("\n");
		extval = EXIT_FAILURE;
		goto exit;
	}

	// itero sulle operazioni specificate dalla linea di comando e le eseguo
	list_for_each(cmdline_operation_list, cmdline_operation) {
		switch (cmdline_operation->operation) {
			case 'w':
			case 'a':
				break;
			case 'W':
				r = write_file_list(cmdline_operation);
				break;
			case 'r':
				r = read_file_list(cmdline_operation);
				break;
			case 'R':
				r = read_n_files(cmdline_operation);
				break;
			case 'l':
				r = lock_file_list(cmdline_operation);
				break;
			case 'u':
				r = unlock_file_list(cmdline_operation);
				break;
			case 'c':
				r = remove_file_list(cmdline_operation);
				break;
		}
		if (r == -1) break;
		/* se è stata specificata l'opzione -t attendo per il tempo specificato prima di inviare l'eventuale 
		   richiesta successiva */
		if (cmdline_operation->time > 0) {
			if (millisleep(cmdline_operation->time) == -1) {
				fprintf(stderr, "\nERR: millisleep (%s)\n", strerror(errno));
			}
		}
		errno = 0;
	}
	
	// invoco la funzione della api per la chiusura della connessione con il server
	r = closeConnection(sockname);
	PRINT("\ncloseConnection(sockname = %s) : %s\n",
		sockname, r == -1 ? errno_to_str(errno) : "OK");
	if (r == -1) {
		extval = EXIT_FAILURE;
		goto exit;
	}

	return 0;

exit:
	return extval;
}