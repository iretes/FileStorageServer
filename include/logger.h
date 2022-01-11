/**
 * @file     logger.h
 * @brief    Interfaccia per il logging delle operazioni. 
 *           Le funzioni definite sono thread safe.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <pthread.h>

/* Dimensione massima di un record del file di log */
#define RECORD_SIZE 1024

/**
 * @struct       logger_t
 * @brief        Oggetto logger
 * 
 * @var file     File di log
 * @var mutex    Mutex per l'accesso in mutua esclusione al file
 */
typedef struct logger {
	FILE* file;
	pthread_mutex_t mutex;
} logger_t;

/**
 * @function               logger_create()
 * @brief                  Crea un oggetto logger, apre il file di log, inizializza la mutex ad esso associata e 
 *                         stampa eventualmente nel file il primo record.
 * 
 * @param log_file_path    Path del file di log
 * @param init_line        Record di intestazione da stampare sul file (se NULL non viene stampato)
 * 
 * @return                 Un oggetto logger in caso di successo,
 *                         NULL in caso di fallimento ed errno settato ad indicare l'errore.
 *                         In caso di fallimento errno può assumere i seguenti valori:
 *                         EINVAL se log_file_path è @c NULL o la sua dimensione è 0
 * @note                   Può fallire e settare errno se si verificano gli errori specificati da malloc(), fopen() e 
 *                         pthread_mutex_init().
 *                         Nel caso di fallimento di pthread_mutexattr_init() errno viene settato con i valori che tale
 *                         funzione ritorna.
 */
logger_t* logger_create(char* log_file_path, char*init_line);

/**
 * @function             log_record()
 * @brief                Scrive un record sul file di log.
 * 
 * @param logger         Oggetto log
 * @param message_fmt    Formato del record come in printf. In testa al record viene aggiunta la data e l'ora correnti.
 * @param ...            Argomenti del record
 * 
 * @return               0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                       In caso di fallimento errno può assumere i seguenti valori:
 *                       EINVAL se logger è @c NULL, log_file_path è @c NULL o la sua dimensione è 0
 * @note                 Può fallire e settare errno se si verificano gli errori specificati da malloc(), time(), 
 *                       localtime_r(), strftime(), vsprintf(), pthread_mutex_lock(), pthread_mutex_unlock().
 *                       Nel caso di fallimento di pthread_mutexattr_lock() o pthread_mutex_unlock() errno viene settato 
 *                       con i valori che tale funzione ritorna.
 */
int log_record(logger_t* logger, const char* message_fmt, ...);

/**
 * @function        logger_destroy()
 * @brief           Distrugge l'oggetto logger deallocando la memoria.
 * 
 * @param logger    L'oggetto logger da distruggere
 */
void logger_destroy(logger_t* logger);

#endif /* LOGGER_H */