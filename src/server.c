#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <list.h>
#include <int_list.h>
#include <hasht.h>
#include <conc_hasht.h>
#include <config_parser.h>
#include <eviction_policy.h>
#include <protocol.h>
#include <util.h>

/**
 * @function 	usage()
 * @brief		Stampa il messaggio di usage
 * 
 * @param prog	Il nome del programma
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
	int extval = EXIT_SUCCESS;

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

	// se il path del file di configurazione non è stato indicato da linea di comando setto il path di default
	if (config_file == NULL) {
		size_t config_file_len = strlen(DEFAULT_CONFIG_PATH) + 1;
		EQNULL_DO(calloc(config_file_len, sizeof(char)), config_file, EXTF);
		strcpy(config_file, DEFAULT_CONFIG_PATH);
	}

	// effettuo il parsing del file di configurazione
	EQNULL_DO(malloc(sizeof(config_t)), config, EXTF);
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
    
	free(config->socket_path);
	free(config->log_file_path);
	free(config);
    return 0;

server_exit:
	if (config_file)
		free(config_file);
	if (config) {
		if (config->socket_path) {
			unlink(config->socket_path);
			free(config->socket_path);
		}
		if (config->log_file_path)
			free(config->log_file_path);
		free(config);
	}
	return extval;
}