/**
 * @file              client_api.h
 * @brief             Api del client. L'implementazione delle funzioni non è thread safe.
 */

#ifndef CLIENT_API_H
#define CLIENT_API_H

#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>

#include <protocol.h>

/* Flag per l'apertura di un file con modalità "create" */
#define O_CREATE 01
/* Flag per l'apertura di un file con modalità "lock" */
#define O_LOCK 10

/**
 * @def               PRINT()
 * @brief             Stampa sullo stdout con il formato fmt se le stampe sono abilitate.
 * 
 * @param fmt         Formato della stampa come in printf
 * @param ...         Argomenti della stampa
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
 * @function          enable_printing()
 * @brief             Abilita le stampe sullo stdout.
 * 
 * @return            0 in caso di successo, -1 se le stampe erano già abilitate.
 */
int enable_printing();

/**
 * @function          is_printing_enable()
 * @brief             Consente di stabilire se le stampe sullo stdout sono abilitate.
 * 
 * @return            @c true se le stampe sono abilitate, @c false altrimenti.
 */
bool is_printing_enable();

/**
 * @function          errno_to_str()
 * @brief             Restitusce una descrizione dell'errno settato dalle funzioni della api.
 * 
 * @param err         Valore dell'errore
 * @return            Una stringa che descrive err in caso di successo, @c NULL se err non è un errore riconosciuto.
 */
char* errno_to_str(int err);

/**
 * @function          openConnection()
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
 *                    ECOMM        se si è verificato un errore lato client che non ha reso possibile effettuare l'operazione
 *                    EINTR        se è stata ricevuta un'interruzione
 *                    EINVAL       se sockname è @c NULL o la sua lunghezza è 0 o è > UNIX_PATH_MAX-1,
 *                                 se msec < 0, se abstime.tv_sec < 0 o abstime.tv_nsec < 0 o >= 1000000000
 *                    EISCONN      se il client è già connesso alla socket
 *                    ETIMEDOUT    se non è stato possibile instaurare una connessione con il server entro il tempo assoluto 
 *                                 abstime
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
 *                    EALREADY     se la connessione non era stata aperta
 *                    ECOMM        se si sono verificati errori lato client che non hanno reso possibile effettuare 
 *                                 l'operazione
 *                    EINVAL       se sockname è @c NULL o la sua lunghezza è 0
 */
int closeConnection(const char* sockname);

/**
 * @function          openFile()
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
 *                    EALREADY     se il server ha risposto che il file è stato già aperto dal client
 *                    EBADF        se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC      se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY        se il server ha risposto di essere troppo occupato
 *                    ECOMM        se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                 l'operazione
 *                    ECONNRESET   se il server ha chiuso la connessione
 *                    EEXIST       se è stato usato il flag O_CREATE e il server ha risposto che il file è già esistente
 *                    EINVAL       se pathname è @c NULL o è lungo 0 o > PATH_MAX-1, se pathname non è un path assoluto o 
 *                                 se contiene ','
 *                    ENAMETOOLONG se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT       se non è stato usato il flag O_CREATE e il server ha risposto che il file non esiste
 *                    EPERM        se il server ha risposto che lo storage ha raggiunto la capacità massima e non è stato 
 *                                 possibile espellere file
 *                    EPROTO       se si sono verificati errori di protocollo
 */
int openFile(const char* pathname, int flags);

/**
 * @function          readFile()
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
 *                    EBADF        se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC      se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY        se il server ha risposto di essere troppo occupato
 *                    ECOMM        se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                 l'operazione
 *                    ECONNRESET   se il server ha chiuso la connessione
 *                    EINVAL       se pathname è @c NULL o è lungo 0 o > PATH_MAX, se pathname non è un path assoluto o 
 *                                 se contiene ',', 
 *                                 se buf è @c NULL o se size è @c NULL
 *                    ENAMETOOLONG se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT       se il server ha risposto che il file non esiste
 *                    EPERM        se il server ha risposto che l'operazioe sul file non è consentita 
 *                                 (il client non ha precedentemente aperto il file o il file è bloccato da un altro client)
 *                    EPROTO       se si sono verificati errori di protocollo
 */
int readFile(const char* pathname, void** buf, size_t* size);

/**
 * @function          readNFiles()
 * @brief             Richiede al server la lettura di N files qualsiasi da memorizzare nella directory dirname lato client. 
 *                    Se il server ha meno di N file disponibili, li invia tutti. Se N <= 0 la richiesta al server è quella 
 *                    di leggere tutti i file memorizzati al suo interno. 
 * @warning           Nel caso in cui il valore ritornato sia MAX_INT il client può aver ricevto un numero di file maggiore o 
 *                    uguale a MAX_INT.
 * 
 * @param N           Il numero di file da leggere, se <= 0 indica una richiesta di lettura di tutti i file memorizzati dal 
 *                    server
 * @param dirname     La directory in cui vengono memorizzati i file ricevuti dal server, 
 *                    se è @c NULL i file ricevuti non vengono memorizzati 
 * 
 * @return            Un valore >= 0 in caso di successo (cioè ritorna il n. di file effettivamente letti), 
 *                    -1 in caso di fallimento ed errno settato ad indicare l'errore. Se dirname non è @c NULL e non è stato 
 *                    possibile scrivere tutti i file nella directory viene ritornato il numero di file ricevuti ed errno
 *                    è settato a EFAULT.
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    EBADRQC      se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY        se il server ha risposto di essere troppo occupato
 *                    ECOMM        se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                 l'operazione
 *                    ECONNRESET   se il server ha chiuso la connessione
 *                    EFAULT       se non è stato possibile scrivere tutti i file ricevuti nella directory dirname
 *                    EPROTO       se si sono verificati errori di protocollo
 */
int readNFiles(int N, const char* dirname);

/**
 * @function          writeFile()
 * @brief             Scrive tutto il file puntato da pathname nel file server. Ritorna successo solo se la precedente 
 *                    operazione, terminata con successo, è stata openFile(pathname, O_CREATE| O_LOCK). Se dirname è diverso 
 *                    da NULL, il file eventualmente spedito dal server perchè espulso dalla cache per far posto al file 
 *                    pathname viene scritto in dirname.
 * 
 * @param pathname    Il path del file da scrivere nel server
 * @param dirname     Il path della directory in cui memorizzare gli eventuali file espulsi dal server
 * 
 * @return            0 in caso di successo, -1 in caso di fallimento con errno settato a indicare l'errore.
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    EBADF        se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC      se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY        se il server ha risposto di essere troppo occupato
 *                    ECOMM        se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                 l'operazione
 *                    ECONNRESET   se il server ha chiuso la connessione
 *                    EFAULT       se non è stato possibile scrivere in dirname tutti i file che il server ha espulso e 
 *                                 inviato
 *                    EFBIG        se il server ha risposto che la size del file è troppo grande perchè possa essere 
 *                                 memorizzato
 *                    EINVAL       se pathname è NULL o la sua lunghezza è 0 o > PATH_MAX-1
 *                                 se pathname non è un path assoluto o contiene ','
 *                                 se la open del file pathname fallisce settando errno con EACCES, EISDIR, ELOOP, 
 *                                 ENAMETOOLONG, ENOENT, ENOTDIR, EOVERFLOW, EINTR
 *                                 se il file pathname non è un file regolare
 *                                 se dirname non è @c NULL e la sua lunghezza è 0 o > PATH_MAX-1
 *                                 se la creazione della directory dirname fallisce settano errno con ENAMETOOLONG, EACCES, 
 *                                 ELOOP, EMLINK, ENOSPC, EROFS
 *                    ENAMETOOLONG se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT       se il server ha risposto che il file non esiste
 *                    EPERM        se il server ha risposto che l'operazioe sul file non è consentita
 *                                 (l'operazione precedente del client richiedente sul file non è stata openFile(pathname, 
 *                                 O_CREATE| O_LOCK)) o che lo storage ha raggiunto la capacità massima e non è stato 
 *                                 possibile espellere file
 *                    EPROTO       se si sono verificati errori di protocollo
 */
int writeFile(const char* pathname, const char* dirname);

/**
 * @function          appendToFile()
 * @brief             Richiesta di scrivere in append al file pathname i size bytes contenuti nel buffer buf. 
 *                    L’operazione di append nel file è garantita essere atomica dal file server. 
 *                    Se dirname è diverso da NULL, il file eventualmente spedito dal server perchè espulso dalla cache 
 *                    per far posto ai nuovi dati di pathname viene scritto in dirname.
 * 
 * @param pathname    Il path del file su cui effettura l'operazione di append
 * @param buf         Il buffer con i byte da scrivere in append
 * @param size        La size del buffer buf
 * @param dirname     Il path della directory in cui memorizzare gli eventuali file espulsi dal server 
 * 
 * @return            0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    EBADF        se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC      se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY        se il server ha risposto di essere troppo occupato
 *                    ECOMM        se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                 l'operazione
 *                    ECONNRESET   se il server ha chiuso la connessione
 *                    EFAULT       se non è stato possibile scrivere in dirname tutti i file che il server ha espulso e 
 *                                 inviato
 *                    EFBIG        se il server ha risposto che il file diverrebbe troppo grande per essere memorizzato
 *                    EINVAL       se pathname è @c NULL o la sua lunghezza è 0 o > PATH_MAX-1
 *                                 se pathname non è un path assoluto o contiene ','
 *                                 se size non è 0 e buf è @c NULL
 *                                 se dirname non è @c NULL e la sua lunghezza è 0 o > PATH_MAX-1
 *                    ENAMETOOLONG se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT       se il server ha risposto che il file non esiste
 *                    EPERM        se il server ha risposto che l'operazioe sul file non è consentita
 *                                 (il client non ha precedentemente aperto il file o il file è bloccato da un altro 
 *                                 client) o che lo storage ha raggiunto la capacità massima e non è stato possibile 
 *                                 espellere file
 *                    EPROTO       se si sono verificati errori di protocollo
 */
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

/**
 * @function          lockFile()
 * @brief             In caso di successo setta il flag O_LOCK al file. Se il file era stato aperto/creato dal client 
 *                    con il flag O_LOCK, oppure se il file non ha il flag O_LOCK settato, l’operazione termina
 *                    immediatamente con successo, altrimenti l’operazione non viene completata fino a quando il flag O_LOCK 
 *                    non viene resettato dal client detentore della lock. Le lock vengono acquisite dai client secondo la 
 *                    politica FIFO.
 * 
 * @param pathname    Il path del file da bloccare
 * 
 * @return            0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    EBADF        se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC      se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY        se il server ha risposto di essere troppo occupato
 *                    ECOMM        se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                 l'operazione
 *                    ECONNRESET   se il server ha chiuso la connessione
 *                    EINVAL       se pathname è @c NULL o la sua lunghezza è 0 o > PATH_MAX-1
 *                                 se pathname non è un path assoluto o contiene ','
 *                    ENAMETOOLONG se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT       se il server ha risposto che il file non esiste
 *                    EPERM        se il server ha risposto che l'operazioe sul file non è consentita 
 *                                 (il client non ha precedentemente aperto il file)
 *                    EPROTO       se si sono verificati errori di protocollo
 */
int lockFile(const char* pathname);

/**
 * @function          unlockFile()
 * @brief             Resetta il flag O_LOCK sul file pathname. L’operazione ha successo solo se il client detiene la lock
 *                    sul file, altrimenti l’operazione termina con errore.
 * 
 * @param pathname    Il path del file da sbloccare
 * 
 * @return            0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore
 *                    In caso di fallimento errno può assumere i seguenti valori:
 *                    EBADF        se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC      se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY        se il server ha risposto di essere troppo occupato
 *                    ECOMM        se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                 l'operazione
 *                    ECONNRESET   se il server ha chiuso la connessione
 *                    EINVAL       se pathname è @c NULL o la sua lunghezza è 0 o > PATH_MAX-1
 *                                 se pathname non è un path assoluto o contiene ','
 *                    ENAMETOOLONG se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT       se il server ha risposto che il file non esiste
 *                    EPERM        se il server ha risposto che l'operazioe sul file non è consentita 
 *                                 (il client non ha precedentemente bloccato il file)
 *                    EPROTO       se si sono verificati errori di protocollo
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
 *                    EBADF        se il server ha risposto che il path del file non è valido (è vuoto o contiene ',')
 *                    EBADRQC      se il server ha risposto che l'operazione richiesta non è stata riconosciuta
 *                    EBUSY        se il server ha risposto di essere troppo occupato
 *                    ECOMM        se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                 l'operazione
 *                    ECONNRESET   se il server ha chiuso la connessione
 *                    EINVAL       se pathname è @c NULL o la sua lunghezza è 0 o > PATH_MAX-1
 *                                 se pathname non è un path assoluto o contiene ','
 *                    ENAMETOOLONG se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT       se il server ha risposto che il file non esiste
 *                    EPERM        se il server ha risposto che l'operazioe sul file non è consentita
 *                                 (il client non ha precedentemente aperto il file)
 *                    EPROTO       se si sono verificati errori di protocollo
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
 *                    ECOMM        se si sono verificati errori lato client che non hanno reso possibile completare 
 *                                 l'operazione
 *                    ECONNRESET   se il server ha chiuso la connessione
 *                    EINVAL       se pathname è @c NULL o la sua lunghezza è 0 o > PATH_MAX-1
 *                                 se pathname non è un path assoluto o contiene ','
 *                    ENAMETOOLONG se il server ha risposto che il path del file è troppo lungo
 *                    ENOENT       se il server ha risposto che il file non esiste
 *                    EPERM        se il server ha risposto che l'operazioe sul file non è consentita
 *                                 (il client non ha bloccato il file)
 *                    EPROTO       se si sono verificati errori di protocollo
 */
int removeFile(const char* pathname);

#endif /* CLIENT_API_H */