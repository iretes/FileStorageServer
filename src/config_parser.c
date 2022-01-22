/**
 * @file    config_parser.c
 * @brief   Implementazione del parsing del file di configurazione
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include <config_parser.h> 
#include <eviction_policy.h>
#include <protocol.h>
#include <util.h>

/** 
 * Controlla che la lunghezza della stringa, se non ammissibile @c goto lbl
 */
#define CHECK_STR_LEN_GOTO(str, lbl) \
	do { \
		if (strlen(str) > PATH_MAX-1) { \
			fprintf(stderr, "ERR: il path '%s' è troppo lungo\n", str); \
			goto lbl; \
		} \
	} while(0); 

/** 
 * Controlla che flag sia false, se non è così @c goto lbl
 */
#define CHECK_REPEATED_GOTO(flag, parname, lbl) \
	do { \
		if (flag) { \
			fprintf(stderr, "ERR: '%s' può essere specificato una sola volta\n", parname); \
			goto lbl; \
		} \
	} while(0);

/** 
 * Controlla che val sia positivo, se non è così @c goto lbl
 */
#define CHECK_NEG_GOTO(val, param, lbl) \
	do { \
		if (val <= 0) { \
			fprintf(stderr, "ERR: '%s' deve essere un numero positivo\n", param); \
			goto lbl; \
		} \
	} while(0);

/** 
 * Controlla che str rappresenti un numero valido, se non è così @c goto lbl
 */
#define CHECK_NUMBER_GOTO(str, num, lbl) \
	do { \
		if (is_number(str, num) != 0) { \
			fprintf(stderr, "ERR: '%s' non è un numero valido\n", str); \
			goto lbl; \
		} \
	} while(0);

/** 
 * Controlla che val sia minore di limit, se non è così @c goto lbl
 */
#define CHECK_GREATER(val, limit, lbl) \
	do { \
		if (val > limit) { \
			fprintf(stderr, "ERR: '%ld' è troppo grande\n", val); \
			goto lbl; \
		} \
	} while(0);
    
/** 
 * Copia la stringa src nella stringa dest, in caso di fallimento @c goto lbl
 */
#define STR_CPY_GOTO(src, dest, lbl) \
	do { \
		size_t len = strlen(src)+1; \
		dest = calloc(len, sizeof(char)); \
		if (!dest) { \
			fprintf(stderr, "ERR: Not enough space/cannot allocate memory"); \
			goto lbl; \
		} \
		strcpy(dest, src); \
	} while(0); \

/** 
 * Controlla che str contenga solo caratteri di spaziatura, se non è così @c goto lbl
 */
#define CHECK_ALL_SPACES_GOTO(str, lbl) \
	do { \
		if ((str)) { \
			int i = 0; \
			while ((str)[i] != '\0' && isspace((str)[i])) \
				i++; \
			if ((str)[i] != '\0') { \
				fprintf(stderr, "ERR: file di configurazione mal formattato\n"); \
				goto lbl; \
			} \
		} \
	} while(0);

config_t* config_init() {
	config_t* config = malloc(sizeof(config_t));
	if (!config)
		return NULL;

	config->n_workers = DEFAULT_N_WORKERS;
	config->dim_workers_queue = DEFAULT_DIM_WORKERS_QUEUE;
	config->max_file_num = DEFAULT_MAX_FILES;
	config->max_bytes = DEFAULT_MAX_BYTES;
	config->max_locks = DEFAULT_MAX_LOCKS;
	config->expected_clients = DEFAULT_EXPECTED_CLIENTS;
	config->socket_path = NULL;
	config->log_file_path = NULL;
	config->eviction_policy = DEFAULT_EVICTION_POLICY;

	return config;
}

void config_destroy(config_t* config) {
	if (!config)
		return;
	if (config->socket_path)
		free(config->socket_path);
	if (config->log_file_path)
		free(config->log_file_path);
	free(config);
}

int config_parser(config_t *config, char* filepath) {
	if (config == NULL || (filepath != NULL && strlen(filepath) == 0)) {
		errno = EINVAL;
		return -1;
	}
	if (filepath == NULL) {
		STR_CPY_GOTO(DEFAULT_SOCKET_PATH, config->socket_path, config_parser_exit);
		STR_CPY_GOTO(DEFAULT_LOG_PATH, config->log_file_path, config_parser_exit);
		return 0;
	}

	// apro il file di configurazione
	FILE* f = NULL;
	f = fopen(filepath, "r");
	if (!f) {
		fprintf(stderr, "ERR: impossibile aprire il file di configurazione '%s'\n", filepath);
		return -1;
	}

	// variabili per stabilire se i parametri sono stati specificati più volte
	bool nworkers_found, workersqueue_found, maxfiles_found, 
	maxbytes_found, maxlocks_found, expclients_found, 
	socket_found, log_found, evpolicy_found;
	nworkers_found = workersqueue_found = maxfiles_found = 
	maxbytes_found = maxlocks_found = expclients_found = 
	socket_found = log_found = evpolicy_found = false;

	char buf[CONFIG_LINE_SIZE] = {0};
	char *param, *value, *tmpstr, *remaining;

	while (fgets(buf, CONFIG_LINE_SIZE, f) != NULL) {
		// la riga letta è un commento
		if (buf[0] == '#')
			continue;

		/* verifico che la linea letta presenti solo caratteri di spaziatura 
		   o che inizi con un carattere che non è un carattere di spaziatura */
		int i = 0;
		while (buf[i] != '\0' && isspace(buf[i]))
			i++;
		if (buf[i] == '\0')
			continue;
		else if (i != 0) {
			fprintf(stderr, "ERR: file di configurazione mal formattato\n");
			goto config_parser_exit;
		}
		// verifico che la stringa presenti i caratteri '=' e ';'
		if (strchr(buf, '=') == NULL || strchr(buf, ';') == NULL) {
			fprintf(stderr, "ERR: file di configurazione mal formattato\n");
			goto config_parser_exit;
		}

		// divido in tokens la stringa
		param = strtok_r(buf, "=", &tmpstr);
		value = strtok_r(NULL, ";", &tmpstr);
		remaining = strtok_r(NULL, "", &tmpstr);

		// verifico che la stringa letta contenga solo caratteri di spaziatura dopo ';'
		if (remaining) {
			CHECK_ALL_SPACES_GOTO(remaining, config_parser_exit);
		}

		// memorizzo e controllo i valori letti
		long num;
		if (strcmp(param, N_WORKERS_STR) == 0) {
			CHECK_REPEATED_GOTO(nworkers_found, N_WORKERS_STR, config_parser_exit);
			CHECK_NUMBER_GOTO(value, &num, config_parser_exit);
			CHECK_NEG_GOTO(num, param, config_parser_exit);
			CHECK_GREATER(num, SIZE_MAX, config_parser_exit);
			config->n_workers = strtol(value, NULL, 10);
			nworkers_found = true;
		}
		else if (strcmp(param, DIM_WORKERS_QUEUE_STR) == 0) {
			CHECK_REPEATED_GOTO(workersqueue_found, DIM_WORKERS_QUEUE_STR, config_parser_exit);
			CHECK_NUMBER_GOTO(value, &num, config_parser_exit);
			CHECK_NEG_GOTO(num, param, config_parser_exit);  
			CHECK_GREATER(num, SIZE_MAX, config_parser_exit);  
			config->dim_workers_queue = strtol(value, NULL, 10);
			workersqueue_found = true;
		}
		else if (strcmp(param, MAX_FILE_NUM_STR) == 0) {
			CHECK_REPEATED_GOTO(maxfiles_found, MAX_FILE_NUM_STR, config_parser_exit);
			CHECK_NUMBER_GOTO(value, &num, config_parser_exit);
			CHECK_NEG_GOTO(num, param, config_parser_exit);  
			CHECK_GREATER(num, SIZE_MAX, config_parser_exit);  
			config->max_file_num = strtol(value, NULL, 10);
			maxfiles_found = true;
		}
		else if (strcmp(param, MAX_BYTES_STR) == 0) {
			CHECK_REPEATED_GOTO(maxbytes_found, MAX_BYTES_STR, config_parser_exit);
			CHECK_NUMBER_GOTO(value, &num, config_parser_exit);
			CHECK_NEG_GOTO(num, param, config_parser_exit);  
			CHECK_GREATER(num, SIZE_MAX, config_parser_exit);  
			config->max_bytes = strtol(value, NULL, 10);
			maxbytes_found = true;
		}
		else if (strcmp(param, MAX_LOCKS_STR) == 0) {
			CHECK_REPEATED_GOTO(maxlocks_found, MAX_LOCKS_STR, config_parser_exit);
			CHECK_NUMBER_GOTO(value, &num, config_parser_exit);
			CHECK_NEG_GOTO(num, param, config_parser_exit);
			CHECK_GREATER(num, SIZE_MAX, config_parser_exit);
			config->max_locks = strtol(value, NULL, 10);
			maxlocks_found = true;
		}
		else if (strcmp(param, EXPECTED_CLIENTS_STR) == 0) {
			CHECK_REPEATED_GOTO(expclients_found, EXPECTED_CLIENTS_STR, config_parser_exit);
			CHECK_NUMBER_GOTO(value, &num, config_parser_exit);
			CHECK_NEG_GOTO(num, param, config_parser_exit);
			CHECK_GREATER(num, SIZE_MAX, config_parser_exit);
			config->expected_clients = strtol(value, NULL, 10);
			expclients_found = true;
		}
		else if (strcmp(param, SOCKET_PATH_STR) == 0) {
			CHECK_REPEATED_GOTO(socket_found, SOCKET_PATH_STR, config_parser_exit);
			CHECK_STR_LEN_GOTO(value, config_parser_exit);
			STR_CPY_GOTO(value, config->socket_path, config_parser_exit);
			socket_found = true;
		}
		else if (strcmp(param, LOG_FILE_STR) == 0) {
			CHECK_REPEATED_GOTO(log_found, LOG_FILE_STR, config_parser_exit);
			CHECK_STR_LEN_GOTO(value, config_parser_exit);
			STR_CPY_GOTO(value, config->log_file_path, config_parser_exit);
			log_found = true;
		}
		else if (strcmp(param, EVICTION_POLICY_STR) == 0) {
			CHECK_REPEATED_GOTO(evpolicy_found, EVICTION_POLICY_STR, config_parser_exit);
			if (strcmp(value, eviction_policy_to_str(FIFO)) == 0) {
				config->eviction_policy = FIFO;
			}
			else if (strcmp(value, eviction_policy_to_str(LRU)) == 0) {
				config->eviction_policy = LRU;
			}
			else if (strcmp(value, eviction_policy_to_str(LFU)) == 0) {
				config->eviction_policy = LFU;
			}
			else if (strcmp(value, eviction_policy_to_str(LW)) == 0) {
				config->eviction_policy = LW;
			}
			else {
				fprintf(stderr, "ERR: '%s' non è una politica di espulsione valida\n", value);
				goto config_parser_exit;
			}
			evpolicy_found = true;
		}
		else {
			fprintf(stderr, "ERR: '%s' non riconosciuto\n", param);
			goto config_parser_exit;
		}
		memset(buf, 0, CONFIG_LINE_SIZE);
	}

	if (!config->socket_path) {
		STR_CPY_GOTO(DEFAULT_SOCKET_PATH, config->socket_path, config_parser_exit);
	}
	if (!config->log_file_path) {
		STR_CPY_GOTO(DEFAULT_LOG_PATH, config->log_file_path, config_parser_exit);
	}

	fclose(f);
	return 0;

config_parser_exit:
	fclose(f);
	return -1;
}