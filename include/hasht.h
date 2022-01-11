/**
 * @file    hasht.h
 * @brief   Interfaccia della tabella hash con liste di trabocco.
 *          Adattamento della tabella hash, fornita durante il corso, di Jakub Kurzak.
 */

#ifndef HASHT_H
#define HASHT_H

/* Fattore di carico della tabella */
#define LOAD_FACTOR 0.75

/**
 * @struct      entry_t
 * @brief       Entry della tabella.
 * 
 * @var key     Chiave
 * @var value   Valore associato alla chiave
 * @var next    Puntatore alla entry successiva del bucket
 */
typedef struct entry {
	void* key;
	void *value;
	struct entry* next;
} entry_t;

/**
 * @struct                  hasht_t
 * @brief                   Oggetto che rappresenta la tabella hash.
 *
 * @var nbuckets            Numero di buckets
 * @var nentries            Numero corrente di entry
 * @var buckets             Array di bucket
 * @var hash_function       Puntatore alla funzione hash
 * @var hash_key_compare    Puntatore alla funzione per paragonare le chiavi delle entry
 */
typedef struct hasht {
	size_t nbuckets;
	size_t nentries;
	entry_t **buckets;
	unsigned int (*hash_function)(void*);
	int (*hash_key_compare)(void*, void*);
} hasht_t;

/**
 * @function                hasht_create()
 * @brief                   Crea un oggetto che rappresenta una tabella hash con liste di trabocco.
 * 
 * @param n_buckets         Numero di bucket della tabella
 * @param hash_function     Puntatore alla funzione hash (se @c NULL verrà utilizzata la funzione hash_pjw())
 * @param hash_key_compare  Puntatore alla funzione per paragonare le chiavi delle entry 
 *                          (se @c NULL verrà effettuato il casting delle chiavi a char* e verrano paragonate con strcmp())
 *
 * @return                  Un oggetto che rappresenta una tabella hash in caso di successo,
 *                          @c NULL in caso di fallimento ed errno settato ad indicare l'errore.
 *                          In caso di fallimento errno può assumere i seguenti valori:
 *                          EINVAL se n_buckets è 0
 * @note                    Può fallire e settare errno se si verificano gli errori specificati da malloc(), hash_create().
 */
hasht_t* hasht_create(size_t n_buckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*));

/**
 * @function            hasht_destroy()
 * @brief               Distrugge l'oggetto che rappresenta la tabella hash deallocando la memoria.
 *                      Se free_key è diversa da @c NULL utilizza tale funzione per deallocare le chiavi.
 *                      Se free_value è diverso da @c NULL utilizza tale funzione per deallocare i valori.
 * 
 * @param ht            Oggetto che rappresenta la tabella hash
 * @param free_key      Puntatore alla funzione per deallocare le chiavi
 * @param free_value    Puntatore alla funzione per deallocare i valori
 */
void hasht_destroy(hasht_t *ht, void (*free_key)(void*), void (*free_value)(void*));

/**
 * @function    hasht_contains()
 * @brief       Consente di stabilire se key è presente nella tabella.
 * 
 * @param ht    Oggetto che rappresenta la tabella hash
 * @param key   Chiave che si intende ricercare
 *
 * @return      1 se la chiave è presente nella tabella, 0 se non è presente,
 *              -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *              In caso di fallimento errno può assumere i seguenti valori:
 *              EINVAL se ht è @c NULL o key è @c NULL 
 */
int hasht_contains(hasht_t *ht, void* key);

/**
 * @function    hasht_get_value()
 * @brief       Restituisce il puntatore al valore associato alla chiave key, se presente nella tabella.
 * 
 * @param ht    Oggetto che rappresenta la tabella hash
 * @param key   Chiave di cui si intende ottenere il valore
 *
 * @return      Il valore associato a key in caso di successo, NULL se key non è presente nella tabella o
 *              in caso di fallimento. In caso di fallimento errno viene settato e può assumere i seguenti valori:
 *              EINVAL se ht è @c NULL o key è @c NULL .
 */
void* hasht_get_value(hasht_t* ht, void* key);

/**
 * @function    hasht_insert()
 * @brief       Inserisce la coppia la chiave key e il valore value nella tabella se key non è già presente.
 * 
 * @param ht    Oggetto che rappresenta la tabella hash
 * @param key   Chiave che si intende inserire nella tabella
 * @param value Valore che si intende inserire nella tabella
 *
 * @return      0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *              In caso di fallimento errno può assumere i seguenti valori:
 *              EINVAL se ht è @c NULL o key è @c NULL
 *              EEXIST se key è già presente nella tabella
 * @note        Può fallire e settare errno se si verificano gli errori specificati da malloc().
 */      
int hasht_insert(hasht_t* ht, void* key, void* value);

/**
 * @function            hasht_delete()
 * @brief               Elimina la chiave uguale a key e il valore ad essa associato dalla tabella. 
 *                      Se free_key è diversa da @c NULL utilizza tale funzione per deallocare la chiave uguale a key. 
 *                      Se free_value è diverso da @c NULL utilizza tale funzione per deallocare il valore associato a key.
 * 
 * @param ht            Oggetto che rappresenta la tabella hash
 * @param key           Chiave che si intende eliminare dalla tabella
 * @param free_key      Puntatore alla funzione per deallocare la chiave uguale key
 * @param free_value    Puntatore alla funzione per deallocare il valore associato a key nella tabella
 *
 * @return              0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                      In caso di fallimento errno può assumere i seguenti valori:
 *                      EINVAL se ht è @c NULL o key è @c NULL
 * @note                Le chiavi vengono paragonate con la funzione specificata in hasht_create().
 */
int hasht_delete(hasht_t* ht, void* key, void (*free_key)(void*), void (*free_value)(void*));

/**
 * @function        hasht_delete_and_get()
 * @brief           Elimina la chiave uguale a key dalla tabella e restituisce il valore ad essa associato.
 *                  Se free_key() è diversa da @c NULL utilizza tale funzione per deallocare la chiave uguale a key.
 * 
 * @param ht        Oggetto che rappresenta la tabella hash
 * @param key       Chiave che si intende eliminare dalla tabella
 * @param free_key  Puntatore alla funzione per deallocare la chiave uguale key
 *
 * @return          Il valore rimosso dalla tabella in caso di successo, 
 *                  NULL in caso di fallimento ed errno settato ad indicare l'errore.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se ht è @c NULL o key è @c NULL
 * @note            Le chiavi vengono paragonate con la funzione specificata in hasht_create().
 */
void* hasht_delete_and_get(hasht_t *ht, void* key, void (*free_key)(void*));

/**
 * @function    hash_pjw()
 * @brief       Funzione hash.
 *              Adattamento della generica funzione hash di Peter Weinberger (PJW).
 * @author      Allen Holub
 * 
 * @param key   La chiave a cui applicare la funzione
 *
 * @return      Il risultato della funzione hash applicata a key.
 */
unsigned int hash_pjw(void* key);

#endif /* HASHT_H */