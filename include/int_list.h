/**
 * @file    int_list.h
 * @brief   Interfaccia della linked list di interi.
 */

#ifndef INT_LIST_H
#define INT_LIST_H

#include <list.h>

/**
 * @struct      int_list_t()
 * @brief       Oggetto che rappresenta una linked list.
 *
 * @var list    Oggetto che rappresenta una linked list generica
 */
typedef struct int_list {
	list_t* list;
} int_list_t;

/**
 * @function    int_list_create()
 * @brief       Crea un oggetto che rappresenta una linked list di interi.
 *
 * @return      Un oggetto che rappresenta una linked list di interi in caso di successo,
 *              @c NULL in caso di fallimento ed errno settato ad indicare l'errore.
 * @note        Può fallire e settare errno se si verificano gli errori specificati da malloc() e da list_create().
 */
int_list_t* int_list_create();

/**
 * @function        int_list_destroy()
 * @brief           Distrugge l'oggetto che rappresenta una linked list di interi deallocando la memoria.
 *
 * @param int_list  Oggetto che rappresenta una linked list di interi
 */
void int_list_destroy(int_list_t* int_list);

/**
 * @function        int_list_tail_insert()
 * @brief           Inserisce data in coda alla lista.
 * 
 * @param int_list  Oggetto che rappresenta una linked list di interi
 * @param data      Intero che si intende inserire nella lista
 *
 * @return          0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se int_list è @c NULL
 * @note            Può fallire e settare errno se si verificano gli errori specificati da malloc() e da list_tail_insert().
*/
int int_list_tail_insert(int_list_t* int_list, int data);

/**
 * @function        int_list_tail_remove()
 * @brief           Rimuove il nodo in coda alla lista e memorizza l'intero in esso contenuto in data.
 * 
 * @param int_list  Oggetto che rappresenta una linked list di interi
 * @param data      Intero rimosso
 *
 * @return          0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se int_list è @c NULL
 * @note            Può fallire e settare errno se si verificano gli errori specificati da list_tail_remove().
*/
int int_list_tail_remove(int_list_t* int_list, int* data);

/**
 * @function        int_list_head_insert()
 * @brief           Inserisce data in testa alla lista.
 * 
 * @param int_list  Oggetto che rappresenta una linked list di interi
 * @param data      Intero da inserire in testa alla lista
 *
 * @return          0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se int_list è @c NULL
 * @note            Può fallire e settare errno se si verificano gli errori specificati da malloc() e da list_head_insert().
*/
int int_list_head_insert(int_list_t* int_list, int data);

/**
 * @function        int_list_head_remove()
 * @brief           Rimuove il nodo in testa alla lista e memorizza l'intero in esso contenuto in data.
 * 
 * @param int_list  Oggetto che rappresenta una linked list di interi
 * @param data      Intero rimosso
 *
 * @return          0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se int_list è @c NULL
 * @note            Può fallire e settare errno se si verificano gli errori specificati da list_head_remove().
*/
int int_list_head_remove(int_list_t* int_list, int* data);

/**
 * @function        int_list_remove()
 * @brief           Rimuove il nodo della lista che contiene data.
 * 
 * @param int_list  Oggetto che rappresenta una linked list di interi
 * @param data      Intero da rimuovere
 *
 * @return          0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se int_list è @c NULL
 * @note            Può fallire e settare errno se si verificano gli errori specificati da list_remove().
*/
int int_list_remove(int_list_t* int_list, int data);

/**
 * @function        int_list_contains()
 * @brief           Consente di stabilire se data è presente nella lista.
 * 
 * @param int_list  Oggetto che rappresenta una linked list di interi
 * @param data      Intero che si intende ricercare
 *
 * @return          1 se data è presente nella lista, 0 se non è presente
 *                  -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se int_list è @c NULL
 * @note            Può fallire e settare errno se si verificano gli errori specificati da list_contains().
*/
int int_list_contains(int_list_t* int_list, int data);

/**
 * @function        int_list_is_empty()
 * @brief           Consente di stabilire se la lista è vuota.
 *
 * @param int_list  Oggetto che rappresenta una linked list di interi
 *
 * @return          1 se la lista è vuota, 0 se non lo è, 
 *                  -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se int_list è @c NULL
 * @note            Può fallire e settare errno se si verificano gli errori specificati da list_is_empty().
 */
int int_list_is_empty(int_list_t* int_list);

/**
 * @function        int_list_get_length()
 * @brief           Restituisce la lughezza della lista.
 * 
 * @param int_list  Oggetto che rappresenta una linked list di interi
 *
 * @return          La lunghezza della lista in caso di successo, 0 ed errno settato in caso di fallimento.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se int_list è @c NULL oppure data è @c NULL
 * @note            Può fallire e settare errno se si verificano gli errori specificati da list_get_length().
*/
size_t int_list_get_length(int_list_t* int_list);

/**
 * @function        int_list_print()
 * @brief           Stampa il contenuto della lista.
 * 
 * @param int_list  Oggetto che rappresenta una linked list di interi
*/
void int_list_print(int_list_t* int_list);

/**
 * @function        int_list_concatenate()
 * @brief           Concatena a int_list1 gli elementi di int_list2.
 *
 * @param int_list1 Oggetto che rappresenta la linked list di interi a cui concatenare elementi
 * @param int_list2 Oggetto che rappresenta la linked list di interi da concatenare
 *
 * @return          0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se int_list1 è @c NULL o int_list2 è @c NULL 
 * @note            Può fallire e settare errno se si verificano gli errori specificati da int_list_tail_insert().
 */
int int_list_concatenate(int_list_t* int_list1, int_list_t* int_list2);

/**
 * @function        int_list_cpy()
 * @brief           Restituisce una copia della lista.
 *
 * @param int_list  Oggetto che rappresenta la linked list di interi
 *
 * @return          Un oggetto che rappresenta una copia di int_list in caso di successo,
 *                  NULL in caso di fallimento ed errno settato ad indicare l'errore.
 *                  In caso di fallimento errno può assumere i seguenti valori:
 *                  EINVAL se int_list1 è @c NULL
 * @note            Può fallire e settare errno se si verificano gli errori specificati da int_list_create() o 
 *                  int_list_tail_insert().
 */
int_list_t* int_list_cpy(int_list_t* int_list);

/**
 * @def             int_list_for_each()
 * @brief           Itera sulla lista di interi.
 *
 * @param int_list  Oggetto che rappresenta la linked list di interi
 * @param d         Intero corrente
 */
#define int_list_for_each(int_list, d) \
	if (int_list != NULL && int_list->list != NULL) \
	for (node_t* node = int_list->list->head; \
		node != NULL && (node->data) != NULL && ((d=*(int*)node->data) >= 0 || (d=*(int*)node->data) < 0); \
		node = node->next)

#endif /* INT_LIST_H */