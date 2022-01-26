/**
 * @file                      conc_hasht.h
 * @brief                     Interfaccia della tabella hash thread safe con liste di trabocco.
 *                            La tabella hash è divisa in segmenti, ad ogni segmento è associata una recursive lock. 
 *                            E' quindi garantita la consistenza dei dati in accessi concorrenti da parte di più thread.
 */

#ifndef CONC_HASHT_H
#define CONC_HASHT_H

#include <hasht.h>
#include <pthread.h>

/**
 * @struct                    conc_hasht_t
 * @brief                     Oggetto che rappresenta la tabella hash thread safe.
 *
 * @var ht                    Oggetto che rappresenta la tabella hash
 * @var nsegments             Numero di segmenti in cui la tabella è suddivisa
 * @var mutexs                Array di mutex associate ai segmenti
 * @var mutex_attrs           Array di attributi delle mutex associate ai segmenti
 */
typedef struct conc_hasht {
	hasht_t* ht;
	size_t nsegments;
	pthread_mutex_t* mutexs;
	pthread_mutexattr_t* mutex_attrs;
} conc_hasht_t;

/**
 * @function                  conc_hasht_create()
 * @brief                     Crea un oggetto che rappresenta la tabella hash thread safe.
 *                            Le mutex allocate associate ai segmenti sono di tipo recursive, garantendo così ad un thread di 
 *                            acquisire più volte la stessa lock.
 *
 * @param n_buckets           Numero di buckets della tabella
 * @param n_segments          Numero di segmenti in cui viene suddivisa tabella,
 *                            se maggiore o uguale a n_buckets verrà suddivisa in n_buckets segmenti
 * @param hash_function       Funzione hash (se @c NULL verrà utilizzata la funzione hash_pjw())
 * @param hash_key_compare    Funzione per paragonare le chiavi delle entry 
 *                            (se @c NULL verrà effettuato il casting delle chiavi a char* e verrano paragonate con strcmp())
 *
 * @return                    Un oggetto che rappresenta la tabella hash thread safe in caso di successo,
 *                            NULL in caso di fallimento ed errno settato ad indicare l'errore.
 *                            In caso di fallimento errno può assumere i seguenti valori:
 *                            EINVAL se n_buckets è 0 o se n_segments è 0
 * @note                      Può fallire e settare errno se si verificano gli errori specificati da malloc(), 
 *                            pthread_mutexattr_init(), pthread_mutexattr_settype().
 *                            Nel caso di fallimento di pthread_mutexattr_init() e pthread_mutexattr_settype() errno viene 
 *                            settato con i valori che tali funzioni ritornano.
 */
conc_hasht_t* conc_hasht_create(size_t n_buckets, size_t n_segments, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*));

/**
 * @function                  conc_hasht_destroy()
 * @brief                     Distrugge l'oggetto che rappresenta la tabella hash thread safe deallocando la memoria.
 *                            Se free_key è diversa da @c NULL utilizza tale funzione per deallocare le chiavi.
 *                            Se free_value è diverso da @c NULL utilizza tale funzione per deallocare i valori.
 * 
 * @param cht                 Oggetto che rappresenta la tabella hash 
 * @param free_key            Funzione per deallocare le chiavi
 * @param free_value          Funzione per deallocare i valori
 */
void conc_hasht_destroy(conc_hasht_t *cht, void (*free_key)(void*), void (*free_value)(void*));

/**
 * @function                  conc_hasht_contains()
 * @brief                     Consente di stabilire se key è presente nella tabella.
 * @warning                   L'accesso alla tabella non è thread safe, per questo si raccomanda di invocare la funzione 
 *                            dopo aver invocato conc_hasht_lock() con gli stessi parametri.
 *
 * @param cht                 Oggetto che rappresenta la tabella hash thread safe
 * @param key                 Chiave che si intende ricercare
 *
 * @return                    1 se la chiave è presente nella tabella, 0 se non è presente,
 *                            -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                            In caso di fallimento errno può assumere i seguenti valori:
 *                            EINVAL se cht è @c NULL o key è @c NULL
 * @note                      Può fallire e settare errno se si verificano gli errori specificati da hasht_contains().
 */
int conc_hasht_contains(conc_hasht_t *cht, void* key);

/**
 * @function                  conc_hasht_atomic_contains()
 * @brief                     Consente di stabilire se key è presente nella tabella accedendo al segmento della tabella in 
 *                            mutua esclusione.
 * @warning                   Se è stata precedentemente invocata conc_hasht_lock() con gli stessi parametri la lock 
 *                            acquisita verrà rilasciata all'uscita della funzione.
 *
 * @param cht                 Oggetto che rappresenta la tabella hash thread safe
 * @param key                 Chiave che si intende ricercare
 *
 * @return                    1 se la chiave è presente nella tabella, 0 se non è presente,
 *                            -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                            In caso di fallimento errno può assumere i seguenti valori:
 *                            EINVAL se cht è @c NULL o key è @c NULL 
 * @note                      Può fallire e settare errno se si verificano gli errori specificati da hasht_contains(), 
 *                            pthread_mutex_lock(), pthread_mutex_unlock(). Nel caso di fallimento di pthread_mutex_lock() e 
 *                            pthread_mutex_unlock() errno viene settato con i valori che tali funzioni ritornano.
 */
int conc_hasht_atomic_contains(conc_hasht_t *cht, void* key);

/**
 * @function                  conc_hasht_lock()
 * @brief                     Acquisce la lock sul segmento a cui appartiene key (o apparterrebbe se non presente), 
 *                            permette perciò di accedere successivamente a quel segmento in mutua esclusione.
 *
 * @param cht                 Oggetto che rappresenta la tabella hash thread safe
 * @param key                 Chiave che permette di identificare il segmento su cui acquisire la lock
 *
 * @return                    0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                            In caso di fallimento errno può assumere i seguenti valori:
 *                            EINVAL se cht è @c NULL o key è @c NULL 
 * @note                      Può fallire e settare errno se si verificano gli errori specificati da 
 *                            pthread_mutex_lock() in tal caso errno viene settato con il valore che pthread_mutex_lock() 
 *                            ritorna.
 */
int conc_hasht_lock(conc_hasht_t* cht, void* key);

/**
 * @function                  conc_hasht_unlock()
 * @brief                     Rilascia la lock sul segmento a cui appartiene key (o apparterrebbe se non presente).
 *
 * @param cht                 Oggetto che rappresenta la tabella hash thread safe
 * @param key                 Chiave che permette di identificare il segmento su cui rilasciare la lock
 *
 * @return                    0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                            In caso di fallimento errno può assumere i seguenti valori:
 *                            EINVAL se cht è @c NULL o key è @c NULL 
 * @note                      Può fallire e settare errno se si verificano gli errori specificati da pthread_mutex_unlock() 
 *                            in tal caso errno viene settato con il valore che pthread_mutex_unlock() ritorna.
 */
int conc_hasht_unlock(conc_hasht_t* cht, void* key);

/**
 * @function                  conc_hasht_get_value()
 * @brief                     Ritorna il puntatore al valore associato a key se key è presente nella tabella.
 * @warning                   L'accesso alla tabella non è thread safe, per questo si raccomanda di invocare la funzione 
 *                            dopo aver invocato conc_hasht_lock() con gli stessi parametri.
 *
 * @param cht                 Oggetto che rappresenta la tabella hash thread safe
 * @param key                 Chiave il cui valore si intende reperire
 * 
 * @return                    Il puntatore al valore associato alla chiave key se essa è presente nella tabella,
 *                            NULL se key non è presente nella tabella o in caso di fallimento.
 *                            In caso di fallimento errno viene settato e può assumere i seguenti valori:
 *                            EINVAL se cht è @c NULL o key è @c NULL 
 * @note                      Può fallire e settare errno se si verificano gli errori specificati da hasht_get_value().
 */
void* conc_hasht_get_value(conc_hasht_t* cht, void* key);

/**
 * @function                  conc_hasht_insert()
 * @brief                     Inserisce la chiave key e il valore value nella tabella se key non è già presente.
 * @warning                   L'accesso alla tabella non è thread safe, per questo si raccomanda di invocare la funzione 
 *                            dopo aver invocato conc_hasht_lock() con cht e key come parametri.
 *
 * @param cht                 Oggetto che rappresenta la tabella hash thread safe
 * @param key                 Chiave che si intende inserire
 * @param value               Valore che si intende inserire
 * 
 * @return                    0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                            In caso di fallimento errno può assumere i seguenti valori:
 *                            EINVAL se cht è @c NULL o key è @c NULL 
 * @note                      Può fallire e settare errno se si verificano gli errori specificati da hasht_insert().
 */
int conc_hasht_insert(conc_hasht_t *cht, void* key, void *value);

/**
 * @function                  conc_hasht_atomic_insert()
 * @brief                     Inserisce la chiave key e il valore value nella tabella se key non è già presente accedendo 
 *                            al segmento della tabella in mutua esclusione.
 * @warning                   Se è stata precedentemente invocata conc_hasht_lock() con gli stessi parametri la lock 
 *                            acquisita verrà rilasciata all'uscita della funzione.
 *
 * @param cht                 Oggetto che rappresenta la tabella hash thread safe
 * @param key                 Chiave che si intende inserire
 * @param value               Valore che si intende inserire
 * 
 * @return                    0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                            In caso di fallimento errno può assumere i seguenti valori:
 *                            EINVAL se cht è @c NULL o key è @c NULL 
 * @note                      Può fallire e settare errno se si verificano gli errori specificati da hasht_insert(), 
 *                            pthread_mutex_lock e pthread_mutex_unlock. Nel caso di fallimento di pthread_mutex_lock() 
 *                            e pthread_mutex_unlock() errno viene settato con i valori che tali funzioni ritornano.
 */
int conc_hasht_atomic_insert(conc_hasht_t *cht, void* key, void *value);

/**
 * @function                  conc_hasht_delete()
 * @brief                     Elimina dalla tabella la entry la cui chiave è uguale a key.
 *                            Se free_key è diversa da @c NULL utilizza tale funzione per deallocare la chiave uguale a key. 
 *                            Se free_value è diverso da @c NULL utilizza tale funzione per deallocare il valore associato 
 *                            a key.
 * @warning                   L'accesso alla tabella non è thread safe, per questo si raccomanda di invocare la funzione
 *                            dopo aver invocato conc_hasht_lock() con cht e key come parametri.
 *
 * @param cht                 Oggetto che rappresenta la tabella hash thread safe
 * @param key                 Chiave che si intende eliminare
 * @param free_key            Funzione per deallocare la chiave uguale a key nella tabella
 * @param free_value          Funzione per deallocare il valore associato alla chiave key
 *
 * @return                    0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                            In caso di fallimento errno può assumere i seguenti valori:
 *                            EINVAL se cht è @c NULL o key è @c NULL
 * @note                      Le chiavi vengono paragonate con la funzione specificata in conc_hasht_create().
 *                            Può fallire e settare errno se si verificano gli errori specificati da hasht_insert().
 */
int conc_hasht_delete(conc_hasht_t *cht, void* key, void (*free_key)(void*), void (*free_value)(void*));

/**
 * @function                  conc_hasht_atomic_delete()
 * @brief                     Elimina dalla tabella la entry la cui chiave è uguale a key accedendo al segmento della 
 *                            tabella in mutua esclusione. Se free_key è diversa da @c NULL utilizza tale funzione per 
 *                            deallocare la chiave uguale a key. Se free_value è diverso da @c NULL utilizza tale funzione 
 *                            per deallocare il valore associato a key.
 * @warning                   Se è stata precedentemente invocata conc_hasht_lock() con gli stessi parametri la lock 
 *                            acquisita verrà rilasciata all'uscita della funzione.
 * 
 * @param cht                 Oggetto che rappresenta la tabella hash thread safe
 * @param key                 Chiave che si intende eliminare
 * @param free_key            Funzione per deallocare la chiave uguale a key nella tabella
 * @param free_value          Funzione per deallocare il valore associato alla chiave key
 *
 * @return                    0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                            In caso di fallimento errno può assumere i seguenti valori:
 *                            EINVAL se cht è @c NULL o key è @c NULL
 * @note                      Le chiavi vengono paragonate con la funzione specificata in conc_hasht_create().
 *                            Può fallire e settare errno se si verificano gli errori specificati da hasht_delete(), 
 *                            pthread_mutex_lock e pthread_mutex_unlock.
 *                            Nel caso di fallimento di pthread_mutex_lock() e pthread_mutex_unlock() errno viene settato 
 *                            con i valori che tali funzioni ritornano.
 */
int conc_hasht_atomic_delete(conc_hasht_t *cht, void* key, void (*free_key)(void*), void (*free_value)(void*));

/**
 * @function                  conc_hasht_delete_and_get()
 * @brief                     Elimina dalla tabella la entry la cui chiave è uguale a key restituendo il riferimento al 
 *                            valore associato alla chiave.
 *                            Se free_key è diversa da @c NULL utilizza tale funzione per deallocare la chiave uguale a key.
 * @warning                   L'accesso alla tabella non è thread safe, per questo si raccomanda di invocare la funzione 
 *                            dopo aver invocato conc_hasht_lock() con cht e key come parametri.
 * 
 * @param cht                 Oggetto che rappresenta la tabella hash thread safe
 * @param key                 Chiave che si intende eliminare
 * @param free_key            Funzione per deallocare la chiave uguale a key nella tabella
 * 
 * @return                    Il puntatore al valore eliminato dalla tabella in caso di successo,
 *                            NULL se key non è presente nella tabella o in caso di fallimento.
 *                            In caso di fallimento errno viene settato e può assumere i seguenti valori:
 *                            EINVAL se cht è @c NULL o key è @c NULL
 * @note                      Le chiavi vengono paragonate con la funzione specificata in conc_hasht_create().
 *                            Può fallire e settare errno se si verificano gli errori specificati da hasht_delete_and_get().
 */
void* conc_hasht_delete_and_get(conc_hasht_t *cht, void* key, void (*free_key)(void*));

/**
 * @function                  conc_hasht_atomic_delete_and_get()
 * @brief                     Elimina dalla tabella la entry la cui chiave è uguale a key accedendo al segmento della 
 *                            tabella in mutua esclusione e restituendo il riferimento al valore associato alla chiave.
 *                            Se free_key è diversa da @c NULL utilizza tale funzione per deallocare la chiave uguale a key.
 * @warning                   Se è stata precedentemente invocata conc_hasht_lock() con gli stessi parametri la lock 
 *                            acquisita verrà rilasciata all'uscita della funzione.
 * 
 * @param cht                 Oggetto che rappresenta la tabella hash thread safe
 * @param key                 Chiave che si intende eliminare
 * @param free_key            Funzione per deallocare la chiave uguale a key nella tabella
 *
 * @return                    Il puntatore al valore eliminato dalla tabella in caso di successo,
 *                            NULL se key non è presente nella tabella o in caso di fallimento.
 *                            In caso di fallimento errno viene settato e può assumere i seguenti valori:
 *                            EINVAL se cht è @c NULL o key è @c NULL
 * @note                      Le chiavi vengono paragonate con la funzione specificata in conc_hasht_create().
 *                            Può fallire e settare errno se si verificano gli errori specificati da hasht_delete_and_get(), 
 *                            pthread_mutex_lock(), pthread_mutex_unlock().
 *                            Nel caso di fallimento di pthread_mutex_lock() e pthread_mutex_unlock() errno viene settato 
 *                            con i valori che tali funzioni ritornano.
 */
void* conc_hasht_atomic_delete_and_get(conc_hasht_t *cht, void* key, void (*free_key)(void*));

#endif /* CONC_HASHT_H */