/**
 * @file            cmdline_parser.c
 * @brief           Implementazione del parsing degli argomenti della linea di comando del client.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

#include <cmdline_parser.h>
#include <cmdline_operation.h>
#include <list.h>
#include <client_api.h>
#include <protocol.h>
#include <util.h>

/**
 * @def             PRINT_NEEDS_ARG()
 * @brief           Stampa sullo stderr che l'opzione option necessita un argomento.
 * 
 * @param option    Il carattere che identifica l'opzione che necessita un argomento
 */
#define PRINT_NEEDS_ARG(option) \
	fprintf(stderr, "ERR: l'opzione -%c necessita un argomento\n", option);

/**
 * @def             PRINT_ONLY_ONCE()
 * @brief           Stampa sullo stderr che l'opzione option può essere specificata una sola volta.
 * 
 * @param option    Il carattere che identifica l'opzione che può essere specificata una sola volta
 */
#define PRINT_ONLY_ONCE(option) \
	fprintf(stderr, "ERR: l'opzione -%c può essere specificata una sola volta\n", option);

/**
 * @def             PRINT_NOT_A_NUMBER()
 * @brief           Stampa sullo stderr che l'argomento arg di un'opzione non è un numero valido.
 * 
 * @param arg       L'argomento di un'opzione che non è un numero valido
 */
#define PRINT_NOT_A_NUMBER(arg) \
	fprintf(stderr, "ERR: '%s' non è un numero valido\n", arg);

/**
 * @def             PRINT_WRONG_ARG()
 * @brief           Stampa sullo stderr che l'argomento arg dell'opzione option non è valido.
 * 
 * @param option    Il carattere che identifica l'opzione il cui argomento non è valido
 * @param arg       L'argomento non valido di un'opzione
 */
#define PRINT_WRONG_ARG(option, arg) \
	fprintf(stderr, "ERR: l'argomento '%s' dell'opzione -%c non è valido\n", arg, option);

/**
 * @function        usage()
 * @brief           Stampa del messaggio di help del client.
 * 
 * @param prog      Il nome del programma
 */
static void usage(char* prog) {
	printf("usage: %s <options>\n", prog);
	printf("options:\n\n"
		"-h			  stampa il messaggio di help\n\n"
		"-f filename		  permette di specificare il path della socket per la\n"
		"			  connessione con il server\n\n"
		"-w dirname[,n=0]	  invia al server una richiesta di scrittura di 'n' file\n"
		"			  presenti nella directory 'dirname'; se n=0, non è\n"
		"			  specificato, è negativo o è maggiore del numero di file\n"
		"			  presenti nella directory viene richiesta la scrittura\n"
		"		          di tutti i file della directory\n\n"
		"-W file1[,file2]	  invia al server una richiesta di scrittura dei file\n"
		"			  specificati\n\n"
		"-a file1,file2,[file3]  invia al server una richiesta di append di file1 alla\n"
		"			  lista di file specificati a seguire\n\n"
		"-D dirname		  permette di specificare la directory in cui verranno\n"
		"			  salvati i file inviati dal server in risposta a -w, -W o -a,\n"
		"			  se la directory non esiste viene creata e vengono\n"
		"			  eventualmente create anche le parent directory\n\n"
		"-r file1[,file2]	  invia al server una richiesta di lettura dei file\n"
		"			  specificati\n\n"
		"-R [n=0]		  invia al server una richiesta di lettura di 'n' file\n"
		"			  qualsiasi; se n=0, non è specificato, è negatvo o è\n"
		"			  maggiore del numero il file memorizzati nel server\n\n"
		"-d dirname		  permette di specificare la directory in cui verrano\n"
		"			  salvati i file inviati dal server in risposta a -r o -R,\n"
		"			  se la directory non esiste viene creata e vengono\n"
		"			  eventualmente create anche le parent directory\n\n"
		"-t time		  permette di specificare il tempo di attesa tra la\n"
		"			  ricezione della risposta del server a una richiesta\n"
		"			  e l'invio di una richiesta successiva\n\n"
		"-l file1[,file2]	  invia al server una richiesta di lock dei file\n"
		"			  specificati\n\n"
		"-u file1[,file2]	  invia al server una richiesta di unlock dei file\n"
		"			  specificati\n\n"
		"-c file1[,file2]	  invia al server una richiesta di eliminazione dei file\n"
		"			  specificati\n\n"
		"-p			  abilita le stampe per ogni operazione\n\n"
		"I path dei file specificati possono essere relativi o assoluti\n");
}

list_t* cmdline_parser(int argc, char* argv[], char** socket_path) {
	int errnosv;
	cmdline_operation_t* cmdline_operation = NULL;
	// inizializzo una lista in cui memorizzare cmdline_operation
	list_t* cmdline_operation_list = list_create(cmdline_operation_cmp, (void (*)(void*))cmdline_operation_destroy);
	if (!cmdline_operation_list) {
		errnosv = errno;
		fprintf(stderr, "ERR: %s\n", strerror(errno));
		goto cmdline_parser_exit;
	}
	// utilizzo getopt per il riconoscimento delle opzioni
	int option;
	while ((option = getopt(argc, argv, ":hpf:w:W:a:D:r:R:d:t:l:u:c:")) != -1) {
		cmdline_operation = NULL;
		switch (option) {
			case 'f': // -f filename
				// controllo se l'opzione è già stata specificata
				if (*socket_path != NULL) {
					PRINT_ONLY_ONCE(option);
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				// controllo se l'opzione non ha argomento
				if (optarg[0] == '-') { 
					PRINT_NEEDS_ARG(option);
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				// copio l'argomento dell'opzione in *socket_path
				size_t socket_path_len = strlen(optarg)+1;
				*socket_path = calloc((socket_path_len), sizeof(char));
				if (*socket_path == NULL) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				strcpy(*socket_path, optarg);
				break;
			case 'R': // -R [n=0]
				cmdline_operation = cmdline_operation_create(option);
				if (cmdline_operation == NULL) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				// controllo se l'opzione non ha argomento
				if (optarg[0] == '-') {
					// aggiungo l'operazione alla lista
					if (list_head_insert(cmdline_operation_list, cmdline_operation) == -1) {
						errnosv = errno;
						fprintf(stderr, "ERR: %s\n", strerror(errno));
						goto cmdline_parser_exit;
					}
					optind -= 1;
					break;
				}
				// controllo se l'argomento dell'opzione è valido
				if (strlen(optarg) < 3 || strstr(optarg, "n=") != optarg) {
					PRINT_WRONG_ARG(option, optarg);
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				long n;
				if (is_number(optarg + 2, &n) != 0 || n > INT_MAX) {
					PRINT_NOT_A_NUMBER(optarg+2);
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				cmdline_operation->n = n;
				// aggiungo l'operazione alla lista
				if (list_head_insert(cmdline_operation_list, cmdline_operation) == -1) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				break;
			case 'w': // w dirname[,n=0] 
				cmdline_operation = cmdline_operation_create(option);
				if (cmdline_operation == NULL) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				// controllo se l'opzione non ha argomento
				if (optarg[0] == '-') { 
					PRINT_NEEDS_ARG(option);
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				// controllo se è stato specificato n e se è valido
				char* comma = strchr(optarg, ',');
				if (comma != NULL) {
					if (comma == optarg) {
						PRINT_WRONG_ARG(option, optarg);
						errnosv = EINVAL;
						goto cmdline_parser_exit;
					}
					size_t len = strlen(comma);
					if (len < 4 || strstr(comma+1, "n=") != (comma+1)) {
						PRINT_WRONG_ARG(option, optarg);
						errnosv = EINVAL;
						goto cmdline_parser_exit;
					}
					long n;
					if (is_number(comma+3, &n) != 0 || n > INT_MAX) {
						PRINT_NOT_A_NUMBER(comma+3);
						errnosv = EINVAL;
						goto cmdline_parser_exit;
					}
					cmdline_operation->n = n;
					optarg[comma-optarg] = '\0';
				}
				// copio dirname
				size_t len = strlen(optarg)+1;
				cmdline_operation->dirname_in = calloc((len), sizeof(char));
				if (cmdline_operation->dirname_in == NULL) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				strcpy(cmdline_operation->dirname_in, optarg);
				// aggiungo l'operazione alla lista
				if (list_head_insert(cmdline_operation_list, cmdline_operation) == -1) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				break;
			case 'W': // -W file1[,file2]
			case 'a': // -a file1,file2[,file3]
			case 'r': // -r file1[,file2]
			case 'l': // -l file1[,file2]
			case 'u': // -u file1[,file2]
			case 'c': // -c file1[,file2]
				cmdline_operation = cmdline_operation_create(option);
				if (cmdline_operation == NULL) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				// controllo se l'opzione non ha argomento
				if (optarg[0] == '-') { 
					PRINT_NEEDS_ARG(option);
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				if (option == 'a') {
					// controllo se sono stati specificati almeno due file separati da ','
					char* comma;
					if ((comma = strchr(optarg, ',')) == NULL) {
						fprintf(stderr, "ERR: l'opzione -a deve avere come argomento una lista di almeno due file\n");
						errnosv = EINVAL;
						goto cmdline_parser_exit;
					}
					if (comma == optarg) {
						PRINT_WRONG_ARG(option, optarg);
						errnosv = EINVAL;
						goto cmdline_parser_exit;
					}
					size_t source_len = comma - optarg;
					size_t optarg_len = strlen(optarg);
					if (optarg_len == source_len+1) {
						fprintf(stderr, "ERR: l'opzione -a deve avere come argomento una lista di almeno due file\n");
						errnosv = EINVAL;
						goto cmdline_parser_exit;
					}
					// copio il primo file
					cmdline_operation->source_file = calloc(sizeof(char), (optarg_len+1));
					if (cmdline_operation->source_file == NULL) {
						errnosv = errno;
						fprintf(stderr, "ERR: %s\n", strerror(errno));
						goto cmdline_parser_exit;
					}
					strcpy(cmdline_operation->source_file, optarg);
					cmdline_operation->source_file[source_len] = '\0';
					optarg = optarg + source_len + 1;
				}
				char* file = NULL;
				// inizializzo una lista di files
				cmdline_operation->files = list_create((int (*)(void*, void*))strcmp, free);
				if (cmdline_operation->files == NULL) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				// aggiungo alla lista di file i tokens
				file = strtok(optarg, ","); 
				char* cpy = NULL;
				while (file != NULL) {
					size_t len = strlen(file)+1;
					cpy = calloc((len), sizeof(char));
					if (cpy == NULL) {
						errnosv = errno;
						fprintf(stderr, "ERR: %s\n", strerror(errno));
						goto cmdline_parser_exit;
					}
					strcpy(cpy, file);
					if (list_tail_insert(cmdline_operation->files, cpy) == -1) {
						errnosv = errno;
						fprintf(stderr, "ERR: %s\n", strerror(errno));
						free(cpy);
						goto cmdline_parser_exit;
					}
					file = strtok(NULL, ",");
				}
				// aggiungo l'operazione alla lista
				if (list_head_insert(cmdline_operation_list, cmdline_operation) == -1) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				break;
			case 'd': // -d dirname
			case 'D': { // -D dirname
				errno = 0;
				// estraggo dalla lista di operazioni l'ultima operazione aggiunta
				cmdline_operation = list_head_remove(cmdline_operation_list);
				if (errno != 0) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				// controllo se l'opzione è stata specificata congiuntamente a un'operazione di lettura o di scrittura
				if (cmdline_operation == NULL || 
					(option == 'd' && (cmdline_operation->operation != 'r' && cmdline_operation->operation != 'R')) ||
					(option == 'D' && (cmdline_operation->operation != 'w' && cmdline_operation->operation != 'W' && 
						cmdline_operation->operation != 'a'))) {
					if (option == 'd') {
						fprintf(stderr, "ERR: l'opzione -d deve essere specificata congiuntamente a -r o -R\n");
					}
					else {
						fprintf(stderr, "ERR: l'opzione -D deve essere specificata congiuntamente a -w,-W o -a\n");
					}
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				if (cmdline_operation->dirname_out != NULL) {
					if (option == 'd') {
						fprintf(stderr, "ERR: l'opzione -d può essere specificata una sola volta congiuntamete a -r o -R\n");
					}
					else {
						fprintf(stderr, "ERR: l'opzione -D può essere specificata una sola volta congiuntamete -w, "
							"-W o -a\n");
					}
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				// controllo se l'opzione non ha argomento
				if (optarg[0] == '-') { 
					PRINT_NEEDS_ARG(option);
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				// copio dirname
				size_t dirname_len = strlen(optarg)+1;
				cmdline_operation->dirname_out = calloc((dirname_len), sizeof(char));
				if (cmdline_operation->dirname_out == NULL) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				// aggiungo l'operazione alla lista
				strcpy(cmdline_operation->dirname_out, optarg);
				if (list_head_insert(cmdline_operation_list, cmdline_operation) == -1) { 
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				break;
			}
			case 't': // -t time
				errno = 0;
				// estraggo dalla lista di operazioni l'ultima operazione aggiunta
				cmdline_operation = list_head_remove(cmdline_operation_list);
				if (errno != 0) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				if (cmdline_operation == NULL) {
					fprintf(stderr, "ERR: l'opzione -t deve essere specificata congiuntamente a un'altra opzione\n");
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				// controllo se l'opzione -t è già stata specificata per l'ultima operazione aggiunta
				if (cmdline_operation->time != -1) {
					fprintf(stderr, "ERR: l'opzione -t può essere specificata una sola volta congiuntamente a un'altra "
						"opzione\n");
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				// controllo se l'opzione non ha argomento
				if (optarg[0] == '-') { 
					PRINT_NEEDS_ARG(option);
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				// controllo se l'argomento è valido
				if (is_number(optarg, &(cmdline_operation->time)) != 0) {
					PRINT_NOT_A_NUMBER(optarg);
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				if (cmdline_operation->time < 0) { // non dovrebbe mai verificarsi
					fprintf(stderr, "ERR: l'argomento di -t non può essere negativo\n");
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				// aggiungo l'operazione alla lista
				if (list_head_insert(cmdline_operation_list, cmdline_operation) == -1) {
					errnosv = errno;
					fprintf(stderr, "ERR: %s\n", strerror(errno));
					goto cmdline_parser_exit;
				}
				break;
			case 'p': // -p
				// abilito le stampe sullo stdout
				if (enable_printing() == -1) {
					PRINT_ONLY_ONCE(option);
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				break;
			case 'h': // -h
				// stampo il messaggio di help
				usage(argv[0]);
				cmdline_operation_destroy(cmdline_operation);
				list_destroy(cmdline_operation_list, LIST_FREE_DATA);
				errno = 0;
				return NULL;
			case ':': 
				// controllo se l'opzione è R, il cui argomento è opzionale
				if (optopt == 'R') {
					cmdline_operation = cmdline_operation_create('R');
					if (cmdline_operation == NULL) {
						errnosv = errno;
						fprintf(stderr, "ERR: %s\n", strerror(errno));
						goto cmdline_parser_exit;
					}
					// aggiungo l'operazione alla lista
					if (list_head_insert(cmdline_operation_list, cmdline_operation) == -1) {
						errnosv = errno;
						fprintf(stderr, "ERR: %s\n", strerror(errno));
						goto cmdline_parser_exit;
					}
				}
				else {
					PRINT_NEEDS_ARG(optopt);
					errnosv = EINVAL;
					goto cmdline_parser_exit;
				}
				break;
			case '?':
				fprintf(stderr, "ERR: opzione -%c non riconosciuta\n", optopt);
				errnosv = EINVAL;
				goto cmdline_parser_exit;
			default: ;
		}
	}
	// se l'opzione -f non è stata specificata copio in *socket_path il valore di default
	if (*socket_path == NULL) {
		size_t def_sock_len = strlen(DEFAULT_SOCKET_PATH)+1;
		*socket_path = calloc(def_sock_len, sizeof(char));
		if (*socket_path == NULL) {
			errnosv = errno;
			fprintf(stderr, "ERR: %s\n", strerror(errno));
			goto cmdline_parser_exit;
		}
		strcpy(*socket_path, DEFAULT_SOCKET_PATH);
	}
	// estraggo l'ultima operazione aggiunta alla lista
	errno = 0;
	cmdline_operation = list_head_remove(cmdline_operation_list);
	if (errno != 0) {
		errnosv = errno;
		fprintf(stderr, "ERR: %s\n", strerror(errno));
		goto cmdline_parser_exit;
	}
	if (cmdline_operation != NULL) {
		// se era stato specificato un tempo di attesa lo setto a 0
		if (cmdline_operation->time > 0)
			cmdline_operation->time = 0;
		// inserisco l'opzione nella lista
		if (list_head_insert(cmdline_operation_list, cmdline_operation) == -1) {
			errnosv = errno;
			fprintf(stderr, "ERR: %s\n", strerror(errno));
			goto cmdline_parser_exit;
		}
	}
	/* inverto la lista in modo che scorrendola dalla testa presenti 
	   le operazioni nell'ordine con cui sono state specificate sulla linea di comando */
	if (list_reverse(cmdline_operation_list) == -1) {
		errnosv = errno;
		fprintf(stderr, "ERR: %s\n", strerror(errno));
		goto cmdline_parser_exit;
	}
	return cmdline_operation_list;

cmdline_parser_exit:
	cmdline_operation_destroy(cmdline_operation);
	if (cmdline_operation_list != NULL)
		list_destroy(cmdline_operation_list, LIST_FREE_DATA);
	errno = errnosv;
	return NULL;
}