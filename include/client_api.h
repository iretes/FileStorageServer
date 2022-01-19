/**
 * @file     client_api.h
 * @brief    Api del client. L'implementazione delle funzioni non è thread safe.
 */

#ifndef CLIENT_API_H
#define CLIENT_API_H

#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include <protocol.h>

/** File descriptor associato al socket */
int g_socket_fd;
/** Path del socket file */
char g_sockname[UNIX_PATH_MAX];
/** Flag che indica se le stampe sullo stdout sono abilitate */
bool print_enable;

/** Flag per l'apertura di un file con modalità "create" */
#define O_CREATE 01
/** Flag per l'apertura di un file con modalità "lock" */
#define O_LOCK 10

/**
 * @def          PRINT()
 * @brief        Stampa sullo stdout con il formato fmt se le stampe sono abilitate.
 * 
 * @param fmt    Formato della stampa come in printf
 * @param ...    Argomenti della stampa
 */
#define PRINT(fmt, ...) \
	do { \
	if (is_printing_enable()) { \
		int errnosv = errno; \
		printf(fmt, ##__VA_ARGS__); \
		errno = errnosv; \
	} \
	} while(0); \

/**
 * @function    enable_printing()
 * @brief       Abilita le stampe sullo stdout.
 * 
 * @return      0 in caso di successo, -1 se le stampe erano già abilitate.
 */
int enable_printing();

/**
 * @function   is_printing_enable()
 * @brief      Consente di stabilire se le stampe sullo stdout sono abilitate.
 * 
 * @return     @c true se le stampe sono abilitate, @c false altrimenti.
 */
bool is_printing_enable();

/**
 * @function     errno_to_str()
 * @brief        Restitusce una descrizione dell'errno settato dalle funzioni della api
 * 
 * @param err    Valore dell'errore
 * @return       Una stringa che descrive err in caso di successo, @c NULL se err non è un errore riconosciuto.
 */
char* errno_to_str(int err);

/**
 * @function openConnection()
 * @brief             Apre una connessione AF_UNIX al socket file sockname. Se il server non accetta immediatamente la 
 *                    richiesta di connessione, la connessione da parte del client viene ripetuta dopo msec millisecondi e 
 *                    fino allo scadere del tempo assoluto abstime. 
 * 
 * @param sockname    Il path del socket file
 * @param msec        Il numero di millisecondi da attendere dopo un tentativo di connessione fallito
 * @param abstime     Il tempo assoluto entro cui è possibile ritentare la connessione
 * 
 * @return            0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    ECOMM      se si è verificato un errore lato client che non ha reso possibile effettuare l'operazione
 *                    EINTR      se è stata ricevuta un'interruzione
 *                    EINVAL     se sockname è @c NULL o la sua lunghezza è 0 o è > UNIX_PATH_MAX-1,
 *                               se msec < 0, se abstime.tv_sec < 0 o abstime.tv_nsec < 0 o >= 1000000000
 *                    EISCONN    se il client è già connesso alla socket
 *                    ETIMEDOUT  se non è stato possibile instaurare una connessione con il server entro il tempo assoluto 
 *                               abstime
 */
int openConnection(const char* sockname, int msec, const struct timespec abstime);

/**
 * @function          closeConnection()
 * @brief             Chiude la connessione AF_UNIX associata al socket file sockname.
 * 
 * @param sockname    Il paht del socket file
 * 
 * @return            0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    EALREADY   se la connessione non era stata aperta
 *                    ECOMM      se si sono verificati errori lato client che non hanno reso possibile effetuare l'operazione
 *                    EINVAL     se sockname è @c NULL o la sua lunghezza è 0
 */
int closeConnection(const char* sockname);

int openFile(const char* pathname, int flags);

int readFile(const char* pathname, void** buf, size_t* size);

int readNFiles(int N, const char* dirname);

int writeFile(const char* pathname, const char* dirname);

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

int lockFile(const char* pathname);

int unlockFile(const char* pathname);

int closeFile(const char* pathname);

int removeFile(const char* pathname);

#endif /* CLIENT_API_H */