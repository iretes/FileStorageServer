#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include <client_api.h>

/** Secondi da dedicare ai tentativi di connessione verso il server */
#define TRY_CONN_FOR_SEC 5
/** Millisecondi da attendere tra un tentativo di connessione verso il server e il successivo */
#define RETRY_CONN_AFTER_MSEC 1000

int main() {
	int r;
	int extval = EXIT_SUCCESS;

	// TODO: parsing degli argomenti

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
	r = openConnection(DEFAULT_SOCKET_PATH, RETRY_CONN_AFTER_MSEC, abstime);
	PRINT("openConnection(sockname = %s) : %s",
		DEFAULT_SOCKET_PATH, r == -1 ? errno_to_str(errno) : "OK");
	if (r == -1) {
		PRINT("\n");
		extval = EXIT_FAILURE;
		goto exit;
	}
	
	// invoco la funzione della api per la chiusura della connessione con il server
	r = closeConnection(DEFAULT_SOCKET_PATH);
	PRINT("\ncloseConnection(sockname = %s) : %s\n",
		DEFAULT_SOCKET_PATH, r == -1 ? errno_to_str(errno) : "OK");
	if (r == -1) {
		extval = EXIT_FAILURE;
		goto exit;
	}

	return 0;

exit:
	return extval;
}