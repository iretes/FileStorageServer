/**
 * @file                  threadpool.h
 * @brief                 Interfaccia del threadpool.
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <stdbool.h>

/**
* @struct                 taskfun_node_t
* @brief                  Generico task che un thread del threadpool deve eseguire.
*
* @var fun                Puntatore alla funzione da eseguire 
*                         (il secondo argomento della funzione è l'identificativo del thread worker)
* @var arg                Argomento della funzione
* @var next               Puntatore al task successivo da eseguire
*/
typedef struct taskfun_node {
	void (*fun)(void *, int);
	void *arg;
	struct taskfun_node* next;
} taskfun_node_t;

/**
 *  @struct               threadpool_t
 *  @brief                Rappresentazione dell'oggetto threadpool.
 * 
 * @var lock              Mutua esclusione nell'accesso all'oggetto
 * @var cond              Variabile di condizione usata per notificare un worker thread 
 * @var threads           Array di workers
 * @var numthreads        Numero di thread (size dell'array threads)
 * @var lhead             Testa della lista dinamica di task pendenti
 * @var ltail             Coda della lista dinamica di task pendenti
 * @var queue_size        Massima size della lista di task pendenti
 * @var taskonthefly      Numero di task attualmente in esecuzione 
 * @var count             Numero di task nella lista di task pendenti
 * @var exiting           true se è iniziato il protocollo di uscita, false atrimenti
 */
typedef struct threadpool {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	pthread_t* threads;
	size_t numthreads;
	taskfun_node_t* lhead;
	taskfun_node_t* ltail;
	size_t queue_size;
	size_t taskonthefly;
	size_t count;
	bool exiting;
} threadpool_t;

/**
* @struct                 worker_args_t
* @brief                  Argomenti del thread worker del pool.
*
* @var pool               Oggetto threadpool
* @var id                 Identificativo del thread worker
*/
typedef struct worker_args_t { 
	threadpool_t* pool;
	int id;
} worker_args_t;

/**
 * @function              threadpool_create()
 * @brief                 Crea un oggetto thread pool.
 * @param numthreads      Il numero di thread del pool
 * @param pending_size    La size della lista di richieste pendenti
 *
 * @return                Un oggetto thread pool oppure @c NULL ed errno settato ad indicare l'errore.
 *                        In caso di fallimento errno può assumere i seguenti valori:
 *                        EINVAL se numthread è 0
 * @note                  Può fallire e settare errno se si verificano gli errori specificati da malloc(), 
 *                        pthread_mutex_init(), pthread_cond_init() e pthread_create().
 *                        Nel caso di fallimento di pthread_mutex_init(), pthread_cond_init() e pthread_create() errno viene 
 *                        settato con i valori che tali funzioni ritornano.
 */
threadpool_t *threadpool_create(size_t numthreads, size_t pending_size);

/**
 * @function              threadpool_destroy()
 * @brief                 Stoppa tutti i thread e distrugge l'oggetto pool deallocando la memoria.
 * @param pool            Oggetto thread pool da liberare
 
 * @return                0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                        In caso di fallimento errno può assumere i seguenti valori:
 *                        EINVAL se pool è @c NULL
 * @note                  Può fallire e settare errno se si verificano gli errori specificati da pthread_mutex_lock(), 
 *                        pthread_mutex_unlock(), pthread_cond_broadcast(), pthread_join().
 *                        Nel caso di fallimento di pthread_mutex_lock(), pthread_mutex_unlock(), pthread_cond_broadcast(), 
 *                        pthread_join() errno viene settato con i valori che tali funzioni ritornano.
 */
int threadpool_destroy(threadpool_t *pool);

/**
 * @function              threadpool_add()
 * @brief                 Aggiunge un task al pool, se ci sono thread liberi il task viene assegnato ad uno di questi, 
 *                        se non ci sono thread liberi si cerca di inserire il task in coda. Se non c'e' posto nella coda 
 *                        interna allora la chiamata fallisce. 
 * 
 * @param pool            L'oggetto thread pool
 * @param fun             Puntatore alla funzione da far eseguire al worker
 * @param arg             Argomento della funzione
 * @return                0 in caso di successo, 1 se non ci sono thread disponibili e/o la coda è piena, -1 in caso di 
 *                        fallimento ed errno settato ad indicare l'errore.
 *                        In caso di fallimento errno può assumere i seguenti valori:
 *                        EINVAL se pool è @c NULL
 * @note                  Può fallire e settare errno se si verificano gli errori specificati da pthread_mutex_lock(), 
 *                        pthread_mutex_unlock() e pthread_cond_signal().
 *                        Nel caso di fallimento di pthread_mutex_lock(), pthread_mutex_unlock() e pthread_cond_signal() 
 *                        errno viene settato con i valori che tali funzioni ritornano.
 */
int threadpool_add(threadpool_t *pool, void (*f)(void *, int), void *arg);

#endif /* THREADPOOL_H */