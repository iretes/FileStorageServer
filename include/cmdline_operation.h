/**
* @file                        cmdline_operation.h
* @brief                       Inerfaccia per le funzioni sulla struttura che rappresenta un'operazione specificata della 
                               linea di comando del client.
*/

#ifndef CMDLINE_OPERATION_H
#define CMDLINE_OPERATION_H

#include <list.h>

/**
 * @struct                     cmdline_operation_t
 * @brief                      Struttura che rappresenta un'operazione specificata della linea di comando del client.
 *
 * @var operation              Operazione richiesta (uno tra i seguenti valori w | W | a | r | R | l | u | c)
 * @var files                  Lista di file su cui è stata richiesta l'operazione (non nulla se l'operazione è 
 *                             -W, -r, -l, -u, -c)
 * @var dirname_in             Directory da cui leggere i file (non nulla se l'operazione è -w)
 * @var dirname_out            Directory in cui memorizzare i file (non nulla se l'operazione è -D o -d)
 * @var source_file            File da cui leggere il contenuto per una richiesta di append (non nulla se l'operazione è -a)
 * @var time                   Tempo da attendere in millisecondi dopo la ricezione della risposta (significativo se 
 *                             l'operazione è -t)
 * @var n                      Valore del parametro n (significativo se l'operazione è -w o -R)
 */
typedef struct cmdline_operation {
	char operation;
	list_t* files;
	char* dirname_in;
	char* dirname_out;
	char* source_file;
	long time;
	int n;
} cmdline_operation_t;

/**
 * @function                   cmdline_operation_create()
 * @brief                      Alloca un puntatore a una struttura che rappresenta un'operazione specificata dalla linea di
 *                             comando del client.
 *
 * @param operation            Operazione richiesta
 *
 * @return                     In caso di successo un puntatore a una struttura che rappresenta le operazioni della linea di
 *                             comando del client, NULL in caso di fallimento ed errno settato ad indicare l'errore.
 *                             In caso di fallimento errno può assumere i seguenti valori:
 *                             EINVAL  Se operation non assume uno dei seguenti valori w | W | a | r | R | l | u | c
 * @note                       Può fallire e settare errno se si verificano gli errori specificati da malloc().
 */
cmdline_operation_t* cmdline_operation_create(char operation);

/**
 * @function                   cmdline_operation_destroy()
 * @brief                      Dealloca il puntatore alla struttura che rappresenta un'operazione specificata dalla linea di
 *                             comando del client, deallocando la memoria.
 *
 * @param cmdline_operation    Il puntatore all'operazione da distruggere
*/
void cmdline_operation_destroy(cmdline_operation_t* cmdline_operation);

/**
 * @function                   cmdline_operation_cmp()
 * @brief                      Confronta due oggetti che rappresentano operazioni della linea di comando.
 *                             Due oggetti che rappresentano operazioni della linea di comando sono uguali se puntano allo 
 *                             stesso indirizzo.
 * @param a                    Prima struttura da confrontare
 * @param b                    Seconda struttura da confrontare
 *
 * @return                     1 se sono uguali, 0 altrimenti.
 */
int cmdline_operation_cmp(void* a, void* b);

/**
 * @function                   cmdline_operation_print()
 * @brief                      Stampa sullo standard output le informazioni di un'operazione specificata dalla linea 
 *                             di comando del client.
 *
 * @param cmdline_operation    Operazione da stampare
 */
void cmdline_operation_print(cmdline_operation_t* cmdline_operation);

#endif /* CMDLINE_OPERATION_H */