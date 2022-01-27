/**
 * @file     logger.c
 * @brief    Implementazione del logging delle operazioni.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>

#include <logger.h>
#include <util.h>

/* Dimensione della stringa per il formato della data e dell'ora da stampare sul log. */
#define TIME_STR_SIZE 20

logger_t* logger_create(char* log_file_path, char* init_line) {
	if (log_file_path == NULL || strlen(log_file_path) == 0) {
		errno = EINVAL;
		return NULL;
	}
	int r;
	// alloco il logger
	logger_t* logger = malloc(sizeof(logger_t));
	if (logger == NULL)
		return NULL;

	// apro il file di log
	logger->file = fopen(log_file_path, "w");
	if (logger->file == NULL) {
		fprintf(stderr, "ERR: Errore nell'apertura in modalità write del file '%s' (errno %d)\n", 
			log_file_path, errno);
		free(logger);
		return NULL;
	}

	// inizializzo la mutex per l'accesso esclusivo al file di log
	if ((r = pthread_mutex_init(&(logger->mutex), NULL)) != 0) {
		fclose(logger->file);
		free(logger);
		errno = r;
		return NULL;
	}

	// stampo la prima riga sul file di log
	if (init_line != NULL)
		fprintf(logger->file, "%s", init_line);
	
	return logger;
}

int log_record(logger_t* logger, const char* message_fmt, ...) {
	if (logger == NULL || message_fmt == NULL || strlen(message_fmt) == 0) {
		errno = EINVAL;
		return -1;
	}
	int r;
	// formatto il tempo corrente
	time_t curr_time;
	struct tm time_info;
	EQM1(time(&curr_time), r);
	if (r == -1)
		return -1;
	struct tm* err;
	EQNULL(localtime_r(&curr_time, &time_info), err);
	if (err == NULL)
		return -1;
	char time_str[TIME_STR_SIZE];
	if (strftime(time_str, sizeof(time_str), "%d-%m-%Y %H:%M:%S", &time_info) != TIME_STR_SIZE-1)
		return -1;

	char record_buffer[RECORD_SIZE];
	va_list args;

	NEQ0(pthread_mutex_lock(&logger->mutex), r);
	if (r != 0) {
		errno = r;
		return -1;
	}

	// costruisco il record da scrivere sul log
	va_start(args, message_fmt);
	if (vsprintf(record_buffer, message_fmt, args) < 0)
		return -1;
	va_end(args);

	// scrivo sul file di log il record
	fprintf(logger->file, "%s,%s\n", time_str, record_buffer);
	fflush(logger->file);

	NEQ0(pthread_mutex_unlock(&logger->mutex), r);
	if (r != 0) {
		errno = r;
		return -1;
	}

	return 0;
}

void logger_destroy(logger_t* logger) {
	if (!logger)
		return;
	fflush(logger->file);
	fclose(logger->file);
	pthread_mutex_destroy(&(logger->mutex));
	free(logger);
}