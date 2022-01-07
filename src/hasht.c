/**
 * @file    hasht.c
 * @brief   Implementazione dell'interfaccia della tabella hash con liste di trabocco
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <limits.h>

#include <hasht.h>

#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))

/**
 * @function string_compare()
 * @brief   Confronta due stringhe
 * 
 * @param a prima stringa da confrontare
 * @param b seconda stringa da confrontare
 *
 * @return  1 se le stringhe sono uguali, 0 altrimenti
 */
static int string_compare(void* a, void* b) {
    return (strcmp( (char*)a, (char*)b ) == 0);
}

hasht_t* hasht_create(size_t n_buckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*)) {
    hasht_t *ht = (hasht_t*) malloc(sizeof(hasht_t));
    if (!ht)
        return NULL;
    if (n_buckets == 0) {
        errno = EINVAL;
        return NULL;
    }

    ht->nentries = 0;
    ht->nbuckets = n_buckets;
    ht->buckets = (entry_t**) malloc(ht->nbuckets * sizeof(entry_t*));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }

    for (int i = 0; i < ht->nbuckets; i ++)
        ht->buckets[i] = NULL;

    ht->hash_function = hash_function ? hash_function : hash_pjw;
    ht->hash_key_compare = hash_key_compare ? hash_key_compare : string_compare; 

    return ht;
}

void hasht_destroy(hasht_t *ht, void (*free_key)(void*), void (*free_value)(void*)) {
    entry_t *bucket, *curr, *next;
    int i;

    if (!ht)
        return;

    if (ht->buckets) {
        for (i=0; i<ht->nbuckets; i++) {
            bucket = ht->buckets[i];
            for (curr = bucket; curr != NULL; ) {
                next = curr->next;
                if (*free_key && curr->key) {
                    (*free_key)(curr->key);
                    curr->key = NULL;
                }
                if (*free_value && curr->value) {
                    (*free_value)(curr->value);
                    curr->value = NULL;
                }
                free(curr);
                curr = next;
            }
        }
    }

    if (ht->buckets) 
        free(ht->buckets);
    free(ht);
    ht = NULL;
}

int hasht_contains(hasht_t *ht, void* key) {
    entry_t* curr;
    unsigned int hash_val;

    if (!ht || !key) {
        errno = EINVAL;
        return -1;
    }

    hash_val = (* ht->hash_function)(key) % ht->nbuckets; 

    for (curr = ht->buckets[hash_val]; curr != NULL; curr = curr->next) {
        if (ht->hash_key_compare(curr->key, key)) {
            return 1;
        }
    }

    return 0;
}

void* hasht_get_value(hasht_t* ht, void* key) {
    entry_t *curr;
    unsigned int hash_val;

    if (!ht || !key) {
        errno = EINVAL;
        return NULL;
    }

    hash_val = (* ht->hash_function)(key) % ht->nbuckets;

    for (curr = ht->buckets[hash_val]; curr != NULL; curr = curr->next)
        if (ht->hash_key_compare(curr->key, key)) {
            return curr->value;
        }

    return NULL;
}

int hasht_insert(hasht_t *ht, void* key, void *value) {
    entry_t *curr;
    unsigned int hash_val;

    if (!ht || !key) {
        errno = EINVAL;
        return -1;
    }

    hash_val = (* ht->hash_function)(key) % ht->nbuckets;

    for (curr = ht->buckets[hash_val]; curr != NULL; curr = curr->next)
        if (ht->hash_key_compare(curr->key, key)) {
            errno = EEXIST;
            return -1; // key esiste già
        }

    // se la chiave non è stata trovata
    curr = (entry_t*) malloc(sizeof(entry_t));
    if (!curr)
        return -1;

    curr->key = key;
    curr->value = value;
    curr->next = ht->buckets[hash_val]; // aggiungo in testo alla lista di trabocco

    ht->buckets[hash_val] = curr;
    ht->nentries++;

    return 0;
}

int hasht_delete(hasht_t *ht, void* key, void (*free_key)(void*), void (*free_value)(void*)) {
    entry_t *curr, *prev;
    unsigned int hash_val;

    if (!ht || !key) {
        errno = EINVAL;
        return -1;
    }

    hash_val = (* ht->hash_function)(key) % ht->nbuckets;

    prev = NULL;
    for (curr = ht->buckets[hash_val]; curr != NULL; )  {
        /* Se trova key e free_key() e free_value() non sono NULL rimuove il nodo dalla lista e dealloca la chiave uguale a 
           key e il valore ad essa associato con le suddette funzioni */
        if (ht->hash_key_compare(curr->key, key)) {
            if (!prev)
                ht->buckets[hash_val] = curr->next;
            else
                prev->next = curr->next;
            if (*free_key && curr->key) {
                (*free_key)(curr->key);
                curr->key = NULL;
            }
            if (*free_value && curr->value) {
                (*free_value)(curr->value);
                curr->value = NULL;
            }
            ht->nentries--;
            free(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -1;
}

void* hasht_delete_and_get(hasht_t *ht, void* key, void (*free_key)(void*)) {
    entry_t *curr, *prev;
    unsigned int hash_val;

    if (!ht || !key) {
        errno = EINVAL;
        return NULL;
    }

    hash_val = (* ht->hash_function)(key) % ht->nbuckets;

    prev = NULL;
    for (curr = ht->buckets[hash_val]; curr != NULL; )  {
        if (ht->hash_key_compare(curr->key, key)) {
            if (!prev)
                ht->buckets[hash_val] = curr->next;
            else
                prev->next = curr->next;
            ht->nentries--;
            void* value = curr->value;
            if (*free_key && curr->key) {
                (*free_key)(curr->key);
                curr->key = NULL;
            }
            free(curr);
            return value;
        }
        prev = curr;
        curr = curr->next;
    }

    return NULL;
}

unsigned int hash_pjw (void* key) {
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if (!datum) 
        return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value);
}