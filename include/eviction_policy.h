/**
 * @file            eviction_policy.h
 * @brief           Header per la definizione delle politiche di espulsione.
 */

#ifndef EVICTION_POLICY_H
#define EVICTION_POLICY_H

/* Fattore per il ridimensionamento dei contatori del numero di utilizzi dei file nel caso in cui venga raggiunto da uno 
   di essi il valore massimo*/
#define RESIZE_OVERFLOW_FACTOR  0.5

/**
 * @enum            eviction_policy_t
 * @brief           Enumerazione delle politiche di espulsione di file dallo storage.
 * 
 * @var FIFO        Politica first in first out
 * @var LRU         Politica least recently used
 * @var LFU         Politica least frequently used
 * @var LW          Politica least weighted
 */
typedef enum eviction_policy {
	FIFO,
	LRU,
	LFU,
	LW
} eviction_policy_t;

/**
 * @function        eviction_policy_to_str()
 * @brief           Restituisce una stringa che rappresenta la politica di espulsione.
 * 
 * @param policy    Politica di espulsione
 * 
 * @return          Una stringa che rappresenta la politica di espulsione in caso di successo,
 *                  NULL in caso di fallimento se policy non è una politica di espulsione valida.
 */
char* eviction_policy_to_str(eviction_policy_t policy);

#endif /* EVICTION_POLICY_H */