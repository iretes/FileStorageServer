/**
 * @file                   client_api.c
 * @brief                  Implementazione dell'api del client.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <client_api.h>
#include <protocol.h>
#include <filesys_util.h>
#include <util.h>

/* File descriptor associato al socket */
static int g_socket_fd = -1;
/* Path del socket file */
static char g_sockname[UNIX_PATH_MAX];
/* Flag che indica se le stampe sullo stdout sono abilitate */
static bool print_enable = false;

char* errno_to_str(int err) {
	switch (err) {
		case EBADRQC:
			return "Operazione non riconosciuta";
		case ENAMETOOLONG:
			return "Path del file troppo lungo";
		case EFBIG:
			return "File troppo grande";
		case EBADF:
			return "Path del file non valido";
		case EEXIST:
			return "File già esistente";
		case ENOENT:
			return "File inesistente";
		case EALREADY:
			return "Operazione già effettuata";
		case EPERM:
			return "Operazione non consentita";
		case EBUSY:
			return "Server occupato";
		case EPROTO:
			return "Errore di protocollo";
		case EINVAL:
			return "Argomenti non validi";
		case ECOMM:
			return "Errore lato client";
		case EINTR:
			return "Ricezione di interruzione";
		case ETIMEDOUT:
			return "Tempo scaduto";
		case ECONNRESET:
			return "Reset della connessione";
		case EISCONN:
			return "Connessione già effettuata";
		case EFAULT:
			return "Non tutti i file ricevuti sono stati scritti su disco";
		case 0:
			return "OK";
		default:
			return NULL;
	}
}

/**
 * @function               set_errno()
 * @brief                  Setta errno in base al codice di risposta ricevuto.
 * 
 * @param code             Il codice di rispsota ricevuto dal server.
 */
static void set_errno(response_code_t code) {
	switch (code) {
		case NOT_RECOGNIZED_OP:
			errno = EBADRQC;
			break;
		case TOO_LONG_PATH:
			errno = ENAMETOOLONG;
			break;
		case TOO_LONG_CONTENT:
			errno = EFBIG;
			break;
		case INVALID_PATH:
			errno = EBADF;
			break;
		case FILE_ALREADY_EXISTS:
			errno = EEXIST;
			break;
		case FILE_NOT_EXISTS:
			errno = ENOENT;
			break;
		case FILE_ALREADY_OPEN:
		case FILE_ALREADY_LOCKED:
			errno = EALREADY;
			break;
		case OPERATION_NOT_PERMITTED:
		case COULD_NOT_EVICT:
			errno = EPERM;
			break;
		case TEMPORARILY_UNAVAILABLE:
			errno = EBUSY;
			break;
		case OK:
			errno = 0;
			break;
		default:
			errno = EPROTO;
	}
}

/**
 * @function               send_reqcode()
 * @brief                  Invia al server il codice di richiesta code.
 * 
 * @param code             Il codice di richiesta da inviare al server
 * @return                 0 in caso di successo, -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                         In caso di fallimento errno può assumere i seguenti valori:
 *                         ECOMM       se si è verificato un errore lato client durante la scrittura sulla socket
 *                         ECONNRESET  se il server ha chiuso la connessione
 */
static int send_reqcode(request_code_t code) {
	int r;
	r = writen(g_socket_fd, &code, sizeof(request_code_t));
	if (r == -1 || r == 0) {
		if (errno == EPIPE)
			errno = ECONNRESET;
		else
			errno = ECOMM;
		return -1;
	}
	return 0;
}

/**
 * @function               send_pathname()
 * @brief                  Invia al server il path di un file.
 * 
 * @param pathname         Path del file da inviare al server
 * @return                 0 in caso di successo, -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                         In caso di fallimento errno può assumere i seguenti valori:
 *                         ECOMM         se si è verificato un errore lato client durante la scrittura sulla socket
 *                         ECONNRESET    se il server ha chiuso la connessione
 */
static int send_pathname(const char* pathname) {
	size_t pathname_len = strlen(pathname) + 1;
	int r;
	// invio al server la dimensione del path del file
	r = writen(g_socket_fd, &pathname_len, sizeof(size_t));
	// in caso di successo invio il path del file
	if (r != -1 && r != 0) {
		r = writen(g_socket_fd, (void*)pathname, pathname_len*sizeof(char));
	}
	if (r == -1 || r == 0) {
		if (errno == EPIPE)
			errno = ECONNRESET;
		else
			errno = ECOMM;
		return -1;
	}
	return 0;
}

/**
 * @function               send_file_content()
 * @brief                  Invia al server il contenuto di un file.
 * 
 * @param buf              Contenuto del file da inviare
 * @param size             Dimensione del contenuto del file
 * 
 * @return                 0 in caso di successo, -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                         In caso di fallimento errno può assumere i seguenti valori:
 *                         ECOMM        se si è verificato un errore lato client durante la scrittura sulla socket
 *                         ECONNRESET   se il server ha chiuso la connessione
 */
static int send_file_content(void* buf, size_t size) {
	int r;
	// invio al server la dimensione del contenuto del file
	r = writen(g_socket_fd, &size, sizeof(size_t));
	// in caso di successo invio il contenuto del file
	if (r != -1 && r != 0 && size != 0) {
		r = writen(g_socket_fd, buf, size);
	}
	if (r == -1 || r == 0) {
		if (errno == EPIPE)
			errno = ECONNRESET;
		else
			errno = ECOMM;
		return -1;
	}
	return 0;
}

/**
 * @function               send_N()
 * @brief                  Invia al server il valore del parametro N.
 * 
 * @param N                Intero da inviare al server
 * 
 * @return                 0 in caso di successo, -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                         In caso di fallimento errno può assumere i seguenti valori:
 *                         ECOMM        se si è verificato un errore lato client durante la scrittura sulla socket
 *                         ECONNRESET   se il server ha chiuso la connessione
 */
static int send_N(int N) {
	int r;
	r = writen(g_socket_fd, &N, sizeof(int));
	if (r == -1 || r == 0) {
		if (errno == EPIPE)
			errno = ECONNRESET;
		else
			errno = ECOMM;
		return -1;
	} 
	return 0;
}

/**
 * @function               receive_respcode()
 * @brief                  Riceve dal server il codice di risposta.
 * 
 * @param code             Codice di risposta ricevuto
 * 
 * @return                 0 in caso di successo, -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                         In caso di fallimento errno può assumere i seguenti valori:
 *                         ECOMM        se si è verificato un errore lato client durante la scrittura sulla socket
 *                         ECONNRESET   se il server ha chiuso la connessione
 */
static int receive_respcode(response_code_t* code) {
	int r;
	r = readn(g_socket_fd, code, sizeof(response_code_t));
	if (r == 0) {
		errno = ECONNRESET;
		return -1;
	}
	else if (r == -1) {
		if (errno != ECONNRESET)
			errno = ECOMM;
		return -1;
	}
	return 0;
}

/**
 * @function               receive_size()
 * @brief                  Riceve dal server il valore di un size_t.
 * 
 * @param size             size_t ricevuto
 * 
 * @return                 0 in caso di successo, -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                         In caso di fallimento errno può assumere i seguenti valori:
 *                         ECOMM         se si è verificato un errore lato client durante la scrittura sulla socket
 *                         ECONNRESET    se il server ha chiuso la connessione
 */
static int receive_size(size_t* size) {
	int r;
	r = readn(g_socket_fd, size, sizeof(size_t));
	if (r == 0) {
		errno = ECONNRESET;
		return -1;
	}
	else if (r == -1) {
		if (errno != ECONNRESET)
			errno = ECOMM;
		return -1;
	}
	return 0;
}

/**
 * @function               receive_pathname()
 * @brief                  Riceve dal server il path di un file.
 * 
 * @param pathname         Path ricevuto
 * @param size             Dimensione del path ricevuto
 * 
 * @return                 0 in caso di successo, -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                         In caso di fallimento errno può assumere i seguenti valori:
 *                         ECOMM         se si è verificato un errore lato client che non ha reso possibile effettuare 
 *                                       l'operazione
 *                         ECONNRESET    se il server ha chiuso la connessione
 *                         EPROTO        se si è verificato un errore di protocollo
 */
static int receive_pathname(char** pathname, size_t* size) {
	int r;
	// ricevo dal server la dimensione del path del file
	if (receive_size(size) == -1)
		return -1;
	
	if (*size == 0) {
		// secondo il protocollo il server non invia 0
		errno = EPROTO;
		return -1;
	}
	
	// alloco una stringa per memorizzare il path
	*pathname = calloc((*size), sizeof(char));
	if (*pathname == NULL) {
		errno = ECOMM;
		return -1;
	} 

	// leggo il path del file
	r = readn(g_socket_fd, *pathname, (*size)*sizeof(char));
	if (r == 0) {
		free(*pathname);
		errno = ECONNRESET;
		return -1;
	}
	else if (r == -1) {
		free(*pathname);
		if (errno != ECONNRESET)
			errno = ECOMM;
		return -1;
	}
	return 0;
}

/**
 * @function               receive_file_content()
 * @brief                  Riceve dal server il contenuto di un file.
 * 
 * @param buf              Contenuto del file ricevuto
 * @param size             Dimensione del contenuto del file ricevuto
 *
 * @return                 0 in caso di successo, -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                         In caso di fallimento errno può assumere i seguenti valori:
 *                         ECOMM         se si è verificato un errore lato client che non ha reso possibile effettuare 
 *                                       l'operazione
 *                         ECONNRESET    se il server ha chiuso la connessione
 */
static int receive_file_content(void** buf, size_t* size) {
	int r;
	// ricevo dal server la dimensione del contenuto file
	if (receive_size(size) == -1)
		return -1;

	// secondo il protocollo il server non invia 0
	if (*size == 0)
		return 0;

	// alloco un buffer per leggere il contenuto del file
	*buf = malloc(*size);
	if (*buf == NULL) {
		errno = ECOMM;
		return -1;
	} 
	// leggo il contenuto del file
	r = readn(g_socket_fd, *buf, *size);
	if (r == 0) {
		free(*buf);
		errno = ECONNRESET;
		return -1;
	}
	else if (r == -1) {
		free(*buf);
		if (errno != ECONNRESET)
			errno = ECOMM;
		return -1;
	}
	return 0;
}

/**
 * @function               receive_files()
 * @brief                  Riceve dal server dei file e li memorizza nella directory dirname ( se diversa da @c NULL ).
 * 
 * @param dirname          La directory in cui memorizzare i file ricevuti, se @c NULL i file ricevuti non vengono memorizzati
 * @param file_received    Il numero di file ricevuti
 * 
 * @return                 0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                         In caso di fallimento errno può assumere i seguenti valori:
 *                         ECOMM       se si è verificato un errore lato client che non ha reso possibile effettuare 
 *                                     l'operazione
 *                         ECONNRESET  se il server ha chiuso la connessione
 *                         EFAULT      se non è stato possibile scrivere tutti i file ricevuti
 *                         EPROTO      se si è verificato un errore di protocollo
 */
static int receive_files(const char* dirname, int* num_file_received) {
	// ricevo dal server il numero di file che intende inviare
	size_t files_to_receive;
	if (receive_size(&files_to_receive) == -1)
		return -1;
	
	int errnosv = 0;
	for (int i = 0; i < files_to_receive; i ++) {
		size_t pathsize_in;
		char* pathname_in;
		size_t size_in;
		void* buf_in = NULL;

		// ricevo dal server il path del file
		if (receive_pathname(&pathname_in, &pathsize_in) == -1)
			return -1;

		// ricevo dal server il contenuto del file
		if (receive_file_content(&buf_in, &size_in) == -1)
			return -1;

		// incremento il numero di file ricevuti
		if (num_file_received != NULL && *num_file_received != INT_MAX) // per evitare overflow
			(*num_file_received) ++;

		/* se dirnam è NULL stampo eventualmente sullo stdout il pathname e i byte ricevuti
		   e proseguo con la ricezione dell'eventuale file successivo */
		if (!dirname) {
			PRINT(" : (%zu byte ricevuti di %s)", size_in, pathname_in);
			free(buf_in);
			free(pathname_in);
			continue;
		}

		// ottengo il nome del file
		char* filename = get_basename(pathname_in);
		if (!filename) {
			free(buf_in);
			free(pathname_in);
			errno = ECOMM;
			return -1;
		}

		// costruisco il path del file concatenando il nome del file al path della directory in cui memorizzarlo
		char* filepath = build_notexisting_path(dirname, filename);
		if (!filepath) {
			free(buf_in);
			free(pathname_in);
			free(filename);
			if (errno == ENOMEM) {
				errno = ECOMM;
				return -1;
			}
			continue;
		}

		// scrivo nel file
		FILE * file_in;
		file_in = fopen(filepath, "w+");
		if (!file_in) {
			errnosv = EFAULT;
		}
		else {
			if (size_in != 0) {
				if (fwrite(buf_in, 1, size_in, file_in) != size_in)
					errnosv = EFAULT;
			}
			if (errnosv != EFAULT) {
				PRINT(" : (%zu byte ricevuti e salvati in %s)", size_in, filepath);
			}
		}
		if (fclose(file_in) == -1)
			errnosv = EFAULT;
		free(buf_in);
		free(pathname_in);
		free(filename);
		free(filepath);
	}
	errno = errnosv;
	if (errno != 0)
		return -1;
	return 0;
}

/**
 * @function               do_simple_request()
 * @brief                  Invia al server il codice di richiesta req_code e il path del file relativo alla richiesta,
 *                         attende la ricezione del codice di risposta e setta errno in base al codice ricevuto.
 * 
 * @param req_code         Codice di richiesta da inviare
 * @param pathname         Path del file da inviare
 * 
 * @return                 0 in caso di successo, -1 in caso di fallimento.
 *                         Errno viene settato nel caso in cui si sono verificati errori che non hanno reso possibile 
 *                         completare l'operazione e nel caso in cui la risposta ricevuta dal server segnala l'esito 
 *                         negativo dell'operazione richiesta.
 *                         Errno può assumere i seguenti valori:
 *                         ECOMM         se si è verificato un errore lato client che non ha reso possibile effettuare 
 *                                       l'operazione
 *                         ECONNRESET    se il server ha chiuso la connessione
 */
static int do_simple_request(request_code_t req_code, const char* pathname) {
	// invio al server il codice di richiesta
	if (send_reqcode(req_code) == -1)
		return -1;

	// invio al server il path del file
	if (send_pathname(pathname) == -1)
		return -1;

	// ricevo dal server il codice di risposta
	response_code_t resp_code;
	if (receive_respcode(&resp_code) == -1)
		return -1;
	
	// setto errno in base al codice di risposta ricevuto
	set_errno(resp_code);

	return 0;
}

int enable_printing() {
	if (print_enable)
		return -1;
	print_enable = true;
	return 0;
}

bool is_printing_enable() {
	return print_enable;
}

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
	if (!sockname || strlen(sockname) > (UNIX_PATH_MAX-1) || strlen(sockname) == 0 ||
		msec < 0 || abstime.tv_sec < 0 || abstime.tv_nsec < 0 || abstime.tv_nsec >= 1000000000) {
		errno = EINVAL;
		return -1;
	} 

	// controllo se il client è già connesso
	if (g_socket_fd != -1) {
		errno = EISCONN;
		return -1;
	}

	// creo il socket lato client
	if ((g_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		errno = ECOMM;
		return -1;
	}

	// inizializzo la struttura necessaria per la connessione
	struct sockaddr_un client_addr;
	memset(&client_addr, '0', sizeof(client_addr));
	client_addr.sun_family = AF_UNIX;    
	strncpy(client_addr.sun_path, sockname, UNIX_PATH_MAX-1);

	// tento la connessione
	int errnosv = 0;
	int r;
	while ((r = connect(g_socket_fd, (struct sockaddr*)&client_addr, sizeof(client_addr))) == -1) {
		errnosv = errno;

		// ottengo il tempo corrente
		struct timespec curr_time = {0, 0};
		if (clock_gettime(CLOCK_REALTIME, &curr_time) == -1) {
			close(g_socket_fd);
			g_socket_fd = -1;
			errno = ECOMM;
			return -1;
		} 

		// controllo se si è verificato un errore che implica il fallimento dell'operazione
		if (errnosv != ENOENT && errnosv != ECONNREFUSED && errnosv != EINTR) {
			close(g_socket_fd);
			g_socket_fd = -1;
			errno = ECOMM;
			return -1;
		}

		// controllo se è stata ricevuta un'interruzione
		if (errnosv == EINTR) {
			if (close(g_socket_fd) == -1)
				errno = ECOMM;
			else
				errno = EINTR;
			g_socket_fd = -1;
			return -1;
		}

		// controllo se il tempo destinato ai tentativi è esaurito
		if (timespeccmp(&curr_time, &abstime, >)) {
			if (close(g_socket_fd) == -1)
				errno = ECOMM;
			else
				errno = ETIMEDOUT;
			g_socket_fd = -1;
			return -1;
		}
		if (msec == 0)
			continue;
		
		// attendo msec millisecondi
		struct timespec towait = {0, 0};
		towait.tv_sec = msec / 1000;
		towait.tv_nsec = (msec % 1000) * 1000000;
		if (nanosleep(&towait, NULL) == -1) {
			errnosv = errno;
			if (close(g_socket_fd) == -1 || errnosv != EINTR)
				errno = ECOMM;
			if (errnosv == EINTR)
				errno = errnosv;
			g_socket_fd = -1;
			return -1;
		}
	}

	// copio sockname
	memset(g_sockname, '\0', UNIX_PATH_MAX);
	strncpy(g_sockname, sockname, UNIX_PATH_MAX-1);

	errno = 0;
	return 0;
}

int closeConnection(const char* sockname) {
	if (!sockname || strlen(sockname) == 0) {
		errno = EINVAL;
		return -1;
	}

	// controllo se la connessione è stata aperta
	if (g_sockname[0] == '\0') {
		errno = EALREADY;
		return -1;
	}
	// controllo se il sockname coincide con quello relativo alla connessione aperta
	if (strcmp(sockname, g_sockname) != 0) {
		errno = EINVAL;
		return -1;
	}

	// chiudo il descrittore associato alla socket
	if (close(g_socket_fd) == -1) {
		errno = ECOMM;
		return -1;
	}
	// resetto il descrittore e il nome della socket
	g_socket_fd = -1;
	g_sockname[0] = '\0';

	return 0;
}

int openFile(const char* pathname, int flags) {
	if (!pathname || strlen(pathname) == 0 || strlen(pathname) > (PATH_MAX-1) ||
		pathname != strchr(pathname, '/') ||strchr(pathname, ',') != NULL) {
		errno = EINVAL;
		return -1;
	}

	// controllo se la connessione è stata aperta
	if (g_socket_fd == -1) {
		errno = ECOMM;
		return -1;
	}
	// setto il codice di richiesta da inviare al server in base al valore di flags
	request_code_t req_code;
	switch (flags) {
		case O_CREATE:
			req_code = OPEN_CREATE;
			break;
		case O_LOCK:
			req_code = OPEN_LOCK;
			break;
		case 0:
			req_code = OPEN_NO_FLAGS;
			break;
		case O_CREATE|O_LOCK:
			req_code = OPEN_CREATE_LOCK;
			break;
		default:
			errno = EINVAL;
			return -1;
	}

	if (do_simple_request(req_code, pathname) == -1 || errno != 0)
		return -1;
	return 0;
}

int readFile(const char* pathname, void** buf, size_t* size) {
	if (!pathname || strlen(pathname) == 0 || strlen(pathname) > (PATH_MAX-1) ||
		strchr(pathname, ',') != NULL || pathname != strchr(pathname, '/') ||
		!buf || !size) {
		errno = EINVAL;
		return -1;
	}

	// controllo se la connessione è stata aperta
	if (g_socket_fd == -1) {
		errno = ECOMM;
		return -1;
	}

	request_code_t req_code = READ;
	if (do_simple_request(req_code, pathname) == -1 || errno != 0)
		return -1;
	
	*buf = NULL;
	if (receive_file_content(buf, size) == -1)
		return -1;
	return 0;
}

int readNFiles(int N, const char* dirname) {
	// controllo se la connessione è stata aperta
	if (g_socket_fd == -1) {
		errno = ECOMM;
		return -1;
	}

	// creo, se non esiste, la directory in cui memorizzare i file ricevuti dal server
	if (dirname && mkdirr(dirname) == -1) {
		switch (errno) {
			case ENAMETOOLONG:
			case EACCES:
			case ELOOP:
			case EMLINK:
			case ENOSPC:
			case EROFS:
				errno = EINVAL;
				break;
			default:
				errno = ECOMM;
		}
		return -1;
	}

	request_code_t req_code = READN;
	if (send_reqcode(req_code) == -1)
		return -1;

	if (send_N(N) == -1)
		return -1;
		
	response_code_t resp_code;
	if (receive_respcode(&resp_code) == -1)
		return -1;

	set_errno(resp_code);

	if (errno != 0)
		return -1;
	
	int files_received = 0;
	if (receive_files(dirname, &files_received) == -1) {
		if (errno != EFAULT)
			return -1;
	}
	
	return files_received;
}

int writeFile(const char* pathname, const char* dirname) {
	if (!pathname || strlen(pathname) == 0 || strlen(pathname) > (PATH_MAX-1) ||
		pathname != strchr(pathname, '/') || strchr(pathname, ',') != NULL ||
		(dirname && strlen(dirname) == 0) || (dirname && strlen(dirname) > (PATH_MAX-1))) {
		errno = EINVAL;
		return -1;
	}

	// controllo se la connessione è stata aperta
	if (g_socket_fd == -1) {
		errno = ECOMM;
		return -1;
	}

	// apro il file pathname
	int errnosv = 0;
	FILE * file;
	file = fopen(pathname, "r+");
	if (!file) {
		switch (errno) {
			case EACCES:
			case EISDIR:
			case ELOOP:
			case ENAMETOOLONG:
			case ENOENT:
			case ENOTDIR:
			case EOVERFLOW:
			case EINTR:
				errno = EINVAL;
				break; 
			default:
				errno = ECOMM;
		}
		return -1;
	}

	// creo, se non esiste, la directory in cui memorizzare i file espulsi dal server
	if (dirname && mkdirr(dirname) == -1) {
		switch (errno) {
			case ENAMETOOLONG:
			case EACCES:
			case ELOOP:
			case EMLINK:
			case ENOSPC:
			case EROFS:
				errnosv = EINVAL;
				break;
			default:
				errnosv = ECOMM;
		}
		fclose(file);
		errno = errnosv;
		return -1; 
	}

	// ricavo la size di pathname
	struct stat statbuf;
	if (stat(pathname, &statbuf) == -1) {
		fclose(file);
		errno = ECOMM;
		return -1;
	}
	size_t buf_size = statbuf.st_size;

	// controllo che pathname sia un file regolare
	if (!S_ISREG(statbuf.st_mode)) {
		fclose(file);
		errno = EINVAL;
		return -1;
	}

	void* buf = NULL;
	if (buf_size != 0) {
		// alloco un buffer per leggere il contenuto del file pathname
		buf = malloc(buf_size);
		if (!buf) {
			fclose(file);
			errno = ECOMM;
			return -1;
		}

		// leggo il contenuto del file pathname
		if (fread(buf, 1, buf_size, file) != buf_size) {
			fclose(file);
			free(buf);
			errno = ECOMM;
			return -1;
		}
	}
	// chiudo il file pathname
	if (fclose(file) == -1) {
		free(buf);
		errno = ECOMM;
		return -1;
	}
	
	request_code_t req_code = WRITE;
	if (send_reqcode(req_code) == -1) {
		free(buf);
		return -1;
	}
	if (send_pathname(pathname) == -1) {
		free(buf);
		return -1;
	}
	if (send_file_content(buf, buf_size) == -1) {
		free(buf);
		return -1;
	}
	if (buf)
		free(buf);
	
	response_code_t resp_code;
	if (receive_respcode(&resp_code) == -1)
		return -1;

	set_errno(resp_code);

	if (errno != 0)
		return -1;

	PRINT(" : %zu bytes scritti", buf_size);

	if (receive_files(dirname, NULL) == -1)
		return -1;
	return 0;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
	if (!pathname || strlen(pathname) == 0 || strlen(pathname) > (PATH_MAX-1) ||
		pathname != strchr(pathname, '/') || strchr(pathname, ',') != NULL ||
		(size != 0 && !buf) ||
		(dirname && strlen(dirname) == 0) ||
		(dirname && strlen(dirname) > (PATH_MAX-1))) {
		errno = EINVAL;
		return -1;
	}

	// controllo se la connessione è stata aperta
	if (g_socket_fd == -1) {
		errno = ECOMM;
		return -1;
	}

	// creo, se non esiste, la directory in cui memorizzare i file espulsi dal server
	if (dirname && mkdirr(dirname) == -1) {
		switch (errno) {
			case ENAMETOOLONG:
			case EACCES:
			case ELOOP:
			case EMLINK:
			case ENOSPC:
			case EROFS:
				errno = EINVAL;
				break;
			default:
				errno = ECOMM;
		}
		return -1;
	}

	request_code_t req_code = APPEND;
	if (send_reqcode(req_code) == -1)
		return -1;
	
	if (send_pathname(pathname) == -1)
		return -1;

	if (send_file_content(buf, size) == -1)
		return -1;

	response_code_t resp_code;
	if (receive_respcode(&resp_code) == -1)
		return -1;

	set_errno(resp_code);

	if (errno != 0)
		return -1;

	PRINT(" : %zu bytes scritti in append", size);
	
	if (receive_files(dirname, NULL) == -1)
		return -1;
	return 0;
}

int lockFile(const char* pathname) {
	if (!pathname || strlen(pathname) == 0 || strlen(pathname) > (PATH_MAX-1) ||
		pathname != strchr(pathname, '/') || strchr(pathname, ',') != NULL) {
		errno = EINVAL;
		return -1;
	}
	
	// controllo se la connessione è stata aperta
	if (g_socket_fd == -1) {
		errno = ECOMM;
		return -1;
	}

	request_code_t req_code = LOCK;
	if (do_simple_request(req_code, pathname) == -1 || (errno != 0 && errno != EALREADY))
		return -1;
	if (errno == EALREADY)
		errno = 0;
	return 0;
}

int unlockFile(const char* pathname) {
	if (!pathname || strlen(pathname) == 0 || strlen(pathname) > (PATH_MAX-1) ||
		pathname != strchr(pathname, '/') || strchr(pathname, ',') != NULL) {
		errno = EINVAL;
		return -1;
	}

	// controllo se la connessione è stata aperta
	if (g_socket_fd == -1) {
		errno = ECOMM;
		return -1;
	}

	request_code_t req_code = UNLOCK;
	if (do_simple_request(req_code, pathname) == -1 || errno != 0)
		return -1;
	return 0;
}

int closeFile(const char* pathname) {
	if (!pathname || strlen(pathname) == 0 || strlen(pathname) > (PATH_MAX-1) ||
		pathname != strchr(pathname, '/') || strchr(pathname, ',') != NULL) {
		errno = EINVAL;
		return -1;
	}

	// controllo se la connessione è stata aperta
	if (g_socket_fd == -1) {
		errno = ECOMM;
		return -1;
	}

	request_code_t req_code = CLOSE;
	if (do_simple_request(req_code, pathname) == -1 || errno != 0)
		return -1;
	return 0;
}

int removeFile(const char* pathname) {
	if (!pathname || strlen(pathname) == 0 || strlen(pathname) > (PATH_MAX-1) ||
		pathname != strchr(pathname, '/') || strchr(pathname, ',') != NULL) {
		errno = EINVAL;
		return -1;
	}

	// controllo se la connessione è stata aperta
	if (g_socket_fd == -1) {
		errno = ECOMM;
		return -1;
	}

	request_code_t req_code = REMOVE;
	if (do_simple_request(req_code, pathname) == -1 || errno != 0)
		return -1;
	return 0;
}