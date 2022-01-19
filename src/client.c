#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include <client_api.h>
#include <cmdline_operation.h>
#include <cmdline_parser.h>
#include <list.h>


/** Secondi da dedicare ai tentativi di connessione verso il server */
#define TRY_CONN_FOR_SEC 5
/** Millisecondi da attendere tra un tentativo di connessione verso il server e il successivo */
#define RETRY_CONN_AFTER_MSEC 1000

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