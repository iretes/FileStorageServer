/**
 * @file     log_format.h
 * @brief    Header per la definizione del formato del file di log
 */

#ifndef LOG_FORMAT_H
#define LOG_FORMAT_H

/* Identificativo del thread master */
#define MASTER_ID 0

/* Stringa di intestazione del file */
#define INIT_LINE "TIME,THREAD_ID,OPERATION,OUTCOME,CLIENT_ID,FILE,BYTES_PROCESSED,CURR_FILES,CURR_BYTES,CURR_CLIENTS\n"
/* Stringa che indica la connessione di un client */
#define NEW_CONNECTION "NEW_CONNECTION"
/* Stringa che indica la disconnessione di un client */
#define CLOSED_CONNECTION "CLOSED_CONNECTION"
/* Stringa che indica che l'operazione richiesta dal client è stata sospesa*/
#define CLIENT_IS_WAITING "CLIENT_IS_WAITING"
/* Stringa che indica il completamento di una richiesta di un'operazione precedentemente sospesa */
#define OP_SUSPENDED "OP_SUSPENDED"
/* Stringa che indica l'avveunta ricezione del segnale SIGHUP */
#define SHUT_DOWN "SHUT_DOWN"
/* Stringa che indica l'avvenuta ricezione del segnale SIGINT o SIGQUIT */
#define SHUT_DOWN_NOW "SHUT_DOWN_NOW"

#endif /* LOG_FORMAT_H */