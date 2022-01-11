/**
 * @file    list.h
 * @brief   Interfaccia della linked list.
 */

#ifndef LIST_H
#define LIST_H

#include <stdlib.h>

/* Indica la volontà di deallocare il dato di un nodo della lista, 
   da passare come secondo parametro in list_destroy() */
#define LIST_FREE_DATA 1
/* Indica la volontà di non deallocare il dato di un nodo della lista, 
   da passare come secondo parametro in list_destroy() */
#define LIST_DO_NOT_FREE_DATA 0

/**
 * @struct      node_t
 * @brief       Nodo della lista.
 *
 * @var data    Dato generico
 * @var next    Puntatore al nodo successivo
 */
typedef struct node {
	void* data;
	struct node* next;
} node_t;

/**
 *  @struct         list_t
 *  @brief          Oggetto che rappresenta una linked list.
 *
 * @var head        Puntatore al nodo in testa alla lista
 * @var tail        Puntatore al nodo in coda alla lista
 * @var len         Lunghezza corrente della lista
 * @var compare     Puntatore alla funzione per paragonare i dati nei nodi
 * @var free_data   Puntatore alla funzione per deallocare i dati nei nodi
 */
typedef struct list {
	node_t* head;
	node_t* tail;
	size_t len;
	int (*compare)(void*, void*);
	void (*free_data)(void*);
} list_t;

/**
 * @function        list_create()
 * @brief           Crea un oggetto che rappresenta una linked list.
 * 
 * @param compare   Puntatore alla funzione per paragonare i dati nei nodi
 * @param free_data Puntatore alla funzione per deallocare i dati nei nodi
 *
 * @return          Un oggetto che rappresenta una linked list in caso di successo,
 *                  @c NULL in caso di fallimento ed errno settato ad indicare l'errore.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se compare è @c NULL oppure free_data è @c NULL
 * @note            Può fallire e settare errno se si verificano gli errori specificati da malloc().
 */
list_t* list_create(int (*compare)(void *, void*), void (*free_data)(void*));

/**
 * @function        list_destroy()
 * @brief           Distrugge l'oggetto che rappresenta una linked list deallocando la memoria.
 *
 * @param list      Oggetto che rappresenta la linked list
 * @param free_data != 0 per indicare la volontà di deallocare i dati nei nodi, 
 *                  = 0 per indicare la volontà di non deallocare i dati nei nodi  
 */
void list_destroy(list_t* list, int free_data);

/**
 * @function    list_tail_insert()
 * @brief       Inserisce il dato in coda alla lista.
 *
 * @param list  Oggetto che rappresenta la linked list
 * @param data  Dato da inserire
 * 
 * @return      0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *              In caso di fallimento errno può assumere i seguenti valori:
 *              EINVAL se list è @c NULL oppure data è @c NULL
 * @note        Può fallire e settare errno se si verificano gli errori specificati da malloc().
*/
int list_tail_insert(list_t* list, void* data);

/**
 * @function    list_tail_remove()
 * @brief       Se la lista non è vuota rimuove il nodo in coda alla lista e restituisce il dato in esso.
 *
 * @param list  Oggetto che rappresenta la linked list
 * 
 * @return      Il dato che era presente in coda alla lista in caso di successo,
 *              NULL se la lista è vuota o in caso di fallimento.
 *              In caso di fallimento viene settato errno e può assumere i seguenti valori:
 *              EINVAL se list è @c NULL
*/
void* list_tail_remove(list_t* list);

/**
 * @function    list_head_insert()
 * @brief       Inserisce il dato in testa alla lista.
 *
 * @param list  Oggetto che rappresenta la linked list
 * @param data  Dato da inserire
 *
 * @return      0 incaso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *              In caso di fallimento errno può assumere i seguenti valori
 *              EINVAL se list è @c NULL oppure data è @c NULL
 * @note        Può fallire e settare errno se si verificano gli errori specificati da malloc().
*/
int list_head_insert(list_t* list, void* data);

/**
 * @function    list_head_remove()
 * @brief       Se la lista non è vuota rimuove il nodo in testa alla lista e restituisce il dato in esso.
 *
 * @param list  Oggetto che rappresenta la linked list
 *
 * @return      Il dato che era presente in testa alla lista in caso di successo,
 *              @c NULL se la lista è vuota o in caso di fallimento.
 *              In caso di fallimento viene settato errno e può assumere i seguenti valori:
 *              EINVAL se list è @c NULL
 */
void* list_head_remove(list_t* list);

/**
 * @function    list_remove_and_get()
 * @brief       Rimuove il nodo con dato uguale a data dalla lista e lo restituisce.
 *
 * @param list  Oggetto che rappresenta la linked list
 * @param data  Il dato che si intende eliminare dalla lista
 * 
 * @return      Il dato rimosso in caso di successo, @c NULL in caso di fallimento o se data non è presente nella lista.
 * @note        I dati vengono paragonati con la funzione specificata in list_create().
 *              In caso di fallimento viene settato errno e può assumere i seguenti valori:
 *              EINVAL se list è @c NULL o data è @c NULL
 */
void* list_remove_and_get(list_t* list, void* data);

/**
 * @function    list_remove()
 * @brief       Rimuove il nodo con dato uguale a data dalla lista.
 * 
 * @param list  Oggetto che rappresenta la linked list
 * @param data  Il dato che si intende eliminare dalla lista
 *
 * @return      0 in caso di successo, -1 se data non è presente nella lista o in caso di fallimento.
 *              In caso di fallimento viene settato errno e può assumere i seguenti valori:
 *              EINVAL se list è @c NULL oppure data è @c NULL
 * @note        I dati vengono paragonati con la funzione specificata in list_create().
 */
int list_remove(list_t* list, void* data);

/**
 * @function    list_contains()
 * @brief       Consente di stabilire se data è presente nella lista.
 *
 * @param list  Oggetto che rappresenta la linked list
 * @param data  Il dato che si intende ricercare
 *
 * @return      1 se il dato è presente nella lista, 0 se non è presente, 
 *              -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *              In caso di fallimento errno può assumere i seguenti valori:
 *              EINVAL se list è @c NULL oppure data è @c NULL
 * @note        I dati vengono paragonati con la funzione specificata in list_create().
*/
int list_contains(list_t* list, void* data);

/**
 * @function    list_is_empty()
 * @brief       Consente di stabilire se la lista è vuota.
 *
 * @param list  Oggetto che rappresenta la linked list
 *
 * @return      1 se la lista è vuota, 0 se non lo è, 
 *              -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *              In caso di fallimento errno può assumere i seguenti valori:
 *              EINVAL se list è @c NULL
 */
int list_is_empty(list_t* list);

/**
 * @function    list_get_length()
 * @brief       Restituisce la lughezza della lista.
 *
 * @param list  Oggetto che rappresenta la linked list
 * 
 * @return      La lunghezza della lista in caso di successo, 0 ed errno settato in caso di fallimento.
 *              In caso di fallimento errno può assumere i seguenti valori:
 *              EINVAL se list è @c NULL oppure data è @c NULL
 */
size_t list_get_length(list_t* list);

/**
 * @function    list_print()
 * @brief       Stampa gli indirizzi dei dati della lista.
 *
 * @param list  Oggetto che rappresenta la linked list
 */
void list_print(list_t* list);

/**
 * @function    list_reverse()
 * @brief       Inverte la lista.
 *
 * @param list  Oggetto che rappresenta la linked list
 *
 * @return      0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *              In caso di fallimento errno può assumere i seguenti valori:
 *              EINVAL se list è @c NULL
 */
int list_reverse(list_t* list);

/**
 * @def         list_for_each()
 * @brief       Itera sulla lista.
 *
 * @param list  Oggetto che rappresenta la linked list
 * @param d     Dato corrente
 */
#define list_for_each(list, d) \
	if (list != NULL) \
	for (node_t* node = list->head; \
		node != NULL && (d = node->data) != NULL; \
		node = node->next)

#endif /* LIST_H */