/**
 * @file     client_api.c
 * @brief    Implementazione dell'api del client
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <client_api.h>
#include <protocol.h>
#include <util.h>

/** File descriptor associato al socket */
int g_socket_fd = -1;
/** Flag che indica se le stampe sullo stdout sono abilitate */
bool print_enable = false;

char* errno_to_str(int err) {
	switch (err) {
		case EALREADY:
			return "Operazione già effettuata";
		case EINVAL:
			return "Argomenti non validi";
		case ECOMM:
			return "Errore lato client";
		case EINTR:
			return "Ricezione di interruzione";
		case ETIMEDOUT:
			return "Tempo scaduto";
		case EISCONN:
			return "Connessione già effettuata";
		case 0:
			return "OK";
		default:
			return NULL;
	}
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