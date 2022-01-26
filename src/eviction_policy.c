/**
 * @file     eviction_policy.c
 * @brief    Implementazione della funzione relativa alle politiche di espulsione.
 */

#include <stddef.h>

#include <eviction_policy.h>

char* eviction_policy_to_str(eviction_policy_t policy) {
	switch (policy) {
		case FIFO:
			return "FIFO";
		case LRU:
			return "LRU";
		case LFU:
			return "LFU";
		case LW:
			return "LW";
		default: 
			return NULL;
	}
}