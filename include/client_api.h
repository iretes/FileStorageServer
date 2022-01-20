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

/**
 * @function openFile()
 * @brief             Richiesta di apertura o di creazione di un file.
 *                    La semantica della openFile dipende dai flags passati come secondo argomento che possono essere 
 *                    O_CREATE ed O_LOCK. In caso di successo, il file viene sempre aperto in lettura e scrittura, 
 *                    ed in particolare le scritture possono avvenire solo in append. Se viene passato il flag O_LOCK 
 *                    (eventualmente in OR con O_CREATE) il file viene aperto e/o creato in modalità locked, 
 *                    che vuol dire che l’unico che può leggere o scrivere il file pathname è il processo che lo ha aperto.
 *                    Il flag O_LOCK può essere esplicitamente resettato utilizzando la chiamata unlockFile().
 * 
 * @param pathname    Path del file da aprire
 * @param flags       Flag con cui aprire il file. 
 *                    Possono essere usati, eventualmente in OR, i flag O_CREATE e O_LOCK.
 *                    Se non si intende specificare alcun flag invocare la funzione con flags = 0.
 * 
 * @return            0 in caso di successo, -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    EALREADY       se il server ha risposto che il file è stato già aperto dal client
 *                    EBADF          se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC        se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY          se il server ha risposto di essere troppo occupato
 *                    ECOMM          se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                  l'operazione
 *                    ECONNRESET     se il server ha chiuso la connessione
 *                    EEXIST         se è stato usato il flag O_CREATE e il server ha risposto che il file è già esistente
 *                    EINVAL         se pathname è @c NULL o è lungo 0 o > PATH_MAX-1, se pathname non è un path assoluto o 
 *                                      se contiene ','
 *                    ENAMETOOLONG   se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT         se non è stato usato il flag O_CREATE e il server ha risposto che il file non esiste
 *                    EPERM          se il server ha risposto che lo storage ha raggiunto la capacità massima e non è stato 
 *                                   possibile espellere file
 *                    EPROTO         se si sono verificati errori di protocollo
 */
int openFile(const char* pathname, int flags);

/**
 * @function readFile()
 * @brief             Legge tutto il contenuto del file dal server (se esiste) ritornando un puntatore ad un'area allocata 
 *                    sullo heap nel parametro buf, mentre size conterrà la dimensione del buffer dati (ossia la dimensione 
 *                    in bytes del file letto). In caso di errore, buf e size non sono validi.
 *
 * @param pathname    Il path del file relativo alla richiesta di lettura
 * @param buf         Il buffer in cui memorizzare il contenuto del file ricevuto dal server
 * @param sie         La size del buffer buf
 * 
 * @return            0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    EBADF         se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC       se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY         se il server ha risposto di essere troppo occupato
 *                    ECOMM         se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                  l'operazione
 *                    ECONNRESET    se il server ha chiuso la connessione
 *                    EINVAL        se pathname è @c NULL o è lungo 0 o > PATH_MAX, se pathname non è un path assoluto o 
 *                                  se contiene ',', 
 *                                  se buf è @c NULL o se size è @c NULL
 *                    ENAMETOOLONG  se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT        se il server ha risposto che il file non esiste
 *                    EPERM         se il server ha risposto che l'operazioe sul file non è consentita 
 *                                  (il client non ha precedentemente aperto il file o il file è bloccato da un altro client)
 *                    EPROTO        se si sono verificati errori di protocollo
 */
int readFile(const char* pathname, void** buf, size_t* size);

int readNFiles(int N, const char* dirname);

int writeFile(const char* pathname, const char* dirname);

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

/**
 * @function lockFile()
 * @brief             In caso di successo setta il flag O_LOCK al file. Se il file era stato aperto/creato dal client 
 *                    con il flag O_LOCK e la, oppure se il file non ha il flag O_LOCK settato, l’operazione termina
 *                    immediatamente con successo, altrimenti l’operazione non viene completata fino a quando il flag O_LOCK 
 *                    non viene resettato dal client detentore della lock. Le lock vengono acquisite dai client secondo la 
 *                    politica FIFO.
 * 
 * @param pathname    Il path del file da bloccare
 * 
 * @return            0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    EBADF         se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC       se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY         se il server ha risposto di essere troppo occupato
 *                    ECOMM         se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                  l'operazione
 *                    ECONNRESET    se il server ha chiuso la connessione
 *                    EINVAL        se pathname è @c NULL o la sua lunghezza è 0 o > PATH_MAX-1
 *                                  se pathname non è un path assoluto o contiene ','
 *                    ENAMETOOLONG  se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT        se il server ha risposto che il file non esiste
 *                    EPERM         se il server ha risposto che l'operazioe sul file non è consentita 
 *                                  (il client non ha precedentemente aperto il file)
 *                    EPROTO        se si sono verificati errori di protocollo
 */
int lockFile(const char* pathname);

/**
 * @function unlockFile()
 * @brief             Resetta il flag O_LOCK sul file pathname. L’operazione ha successo solo se il client detiene la lock
 *                    sul file, altrimenti l’operazione termina con errore.
 * 
 * @param pathname    Il path del file da sbloccare
 * 
 * @return            0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    EBADF se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY se il server ha risposto di essere troppo occupato
 *                    ECOMM se si sono verificati errori lato client che non hanno reso possibile completare l'operazione
 *                    ECONNRESET se il server ha chiuso la connessione
 *                    EINVAL se pathname è @c NULL o la sua lunghezza è 0 o > PATH_MAX-1
 *                           se pathname non è un path assoluto o contiene ','
 *                    ENAMETOOLONG se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT se il server ha risposto che il file non esiste
 *                    EPERM se il server ha risposto che l'operazioe sul file non è consentita 
 *                          (il client non ha precedentemente bloccato il file)
 *                    EPROTO se si sono verificati errori di protocollo
 */
int unlockFile(const char* pathname);

/**
 * @function          closeFile()
 * @brief             Richiesta di chiusura del file puntato da pathname.
 *                    Eventuali operazioni sul file dopo closeFile() falliscono.
 * 
 * @param pathname    Il path del file da chiudere
 * 
 * @return            0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    EBADF         se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC       se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY         se il server ha risposto di essere troppo occupato
 *                    ECOMM         se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                  l'operazione
 *                    ECONNRESET    se il server ha chiuso la connessione
 *                    EINVAL        se pathname è @c NULL o la sua lunghezza è 0 o > PATH_MAX-1
 *                                  se pathname non è un path assoluto o contiene ','
 *                    ENAMETOOLONG  se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT        se il server ha risposto che il file non esiste
 *                    EPERM         se il server ha risposto che l'operazioe sul file non è consentita
 *                                  (il client non ha precedentemente aperto il file)
 *                    EPROTO        se si sono verificati errori di protocollo
 */
int closeFile(const char* pathname);

/**
 * @function          removeFile()
 * @brief             Rimuove il file cancellandolo dal file storage server.
 *                    L’operazione fallisce se il file non è in stato locked, o è bloccato da un altro client.
 * 
 * @param pathname    Il path del file da rimuovere
 * 
 * @return            0 in caso di successo, -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    EBADF        se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC      se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY        se il server ha risposto di essere troppo occupato
 *                    ECOMM        se si sono verificati errori lato client che non hanno reso possibile completare l'operazione
 *                    ECONNRESET   se il server ha chiuso la connessione
 *                    EINVAL       se pathname è @c NULL o la sua lunghezza è 0 o > PATH_MAX-1
 *                                 se pathname non è un path assoluto o contiene ','
 *                    ENAMETOOLONG se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT       se il server ha risposto che il file non esiste
 *                    EPERM        se il server ha risposto che l'operazioe sul file non è consentita
 *                                 (il client non ha bloccato il file)
 *                    EPROTO se si sono verificati errori di protocollo
 */
int removeFile(const char* pathname);

#endif /* CLIENT_API_H */