/**
 * @file    conc_hasht.c
 * @brief   Implementazione dell'interfaccia della tabella hash thread safe con liste di trabocco
 */

#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include <hasht.h>
#include <conc_hasht.h>

conc_hasht_t* conc_hasht_create(size_t n_buckets, size_t n_segments, unsigned int (*hash_function)(void*), 
	int (*hash_key_compare)(void*, void*)) {
	int r, errnosv;
	conc_hasht_t *cht = (conc_hasht_t*) malloc(sizeof(conc_hasht_t));
	if (!cht)
		return NULL;
	if (n_buckets == 0 || n_segments == 0) {
		free(cht);
		errno = EINVAL;
		return NULL;
	}

	cht->ht = hasht_create(n_buckets, hash_function, hash_key_compare);
	if (!cht->ht)
		return NULL;

	cht->nsegments = n_segments > cht->ht->nbuckets? cht->ht->nbuckets : n_segments;

	// alloco l'array di mutex associate ai segmenti
	cht->mutexs = (pthread_mutex_t*) malloc(cht->nsegments * sizeof(pthread_mutex_t));
	if (!cht->mutexs) {
		errnosv = errno;
		hasht_destroy(cht->ht, NULL, NULL);
		free(cht);
		errno = errnosv;
		return NULL;
	}

	// alloco l'array di attributi delle mutex
	cht->mutex_attrs = (pthread_mutexattr_t*) malloc(cht->nsegments * sizeof(pthread_mutexattr_t));
	if (!cht->mutex_attrs) {
		errnosv = errno;
		hasht_destroy(cht->ht, NULL, NULL);
		free(cht->mutexs);
		free(cht);
		errno = errnosv;
		return NULL;
	}

	// inizializzo gli attributi delle mutex, le mutex e setto gli attributi delle mutex
	int i;
	for (i = 0; i < cht->nsegments; i ++) {
		if ((r = pthread_mutexattr_init(&(cht->mutex_attrs[i]))) != 0)
			break;
		if ((r = pthread_mutexattr_settype(&(cht->mutex_attrs[i]), PTHREAD_MUTEX_RECURSIVE)) != 0)
			break;
		if ((r = pthread_mutex_init(&(cht->mutexs[i]), &(cht->mutex_attrs[i]))) != 0) {
			pthread_mutexattr_destroy(&(cht->mutex_attrs[i]));
			break;
		}
	}

	if (r == 0)
		return cht;

	// in caso di fallimento durante l'inizializzazione dealloco quanto allocato precedentemente
	for (int j = 0; j < i; j ++) {
		pthread_mutex_destroy(&(cht->mutexs[j]));
		pthread_mutexattr_destroy(&(cht->mutex_attrs[j]));
	}
	hasht_destroy(cht->ht, NULL, NULL);
	free(cht->mutexs);
	free(cht);
	errno = r;
	return NULL;
}

void conc_hasht_destroy(conc_hasht_t *cht, void (*free_key)(void*), void (*free_value)(void*)) {
	if (!cht)
		return;

	hasht_destroy(cht->ht, free_key, free_value);

	for (int i = 0; i < cht->nsegments; i ++) {
		if (cht->mutexs)
			pthread_mutex_destroy(&(cht->mutexs[i]));
		if (cht->mutex_attrs)
			pthread_mutexattr_destroy(&(cht->mutex_attrs[i]));
	}

	if (cht->mutexs) 
		free(cht->mutexs);
	if (cht->mutex_attrs)
		free(cht->mutex_attrs);
	free(cht);
}

int conc_hasht_contains(conc_hasht_t *cht, void* key) {
	if (!cht || !key) {
		errno = EINVAL;
		return -1;
	}
	return hasht_contains(cht->ht, key);
}

int conc_hasht_atomic_contains(conc_hasht_t *cht, void* key) {
	int r;
	int contained = 0;
	unsigned int hash_val;
	unsigned int mutex_idx;

	if (!cht || !key) {
		errno = EINVAL;
		return -1;
	}

	// calcolo il segmento in cui si trova key
	hash_val = (* cht->ht->hash_function)(key) % cht->ht->nbuckets; 
	mutex_idx = cht->nsegments == 1 ? 0 : hash_val % cht->nsegments; 

	// acquisisco la lock associata al segmento
	r = pthread_mutex_lock(&cht->mutexs[mutex_idx]);
	if (r != 0) {
		errno = r;
		return -1;
	}

	contained = hasht_contains(cht->ht, key);

	// rilascio la lock associata al segmento
	r = pthread_mutex_unlock(&cht->mutexs[mutex_idx]);
	if (r != 0) {
		errno = r;
		return -1;
	}

	return contained;
}

int conc_hasht_lock(conc_hasht_t* cht, void* key) {
	int r;
	unsigned int hash_val;
	unsigned int mutex_idx;

	if (!cht || !key) {
		errno = EINVAL;
		return -1;
	}

	// calcolo il segmento in cui si trova key
	hash_val = (* cht->ht->hash_function)(key) % cht->ht->nbuckets;
	mutex_idx = cht->nsegments == 1 ? 0 : hash_val % cht->nsegments; 

	// acquisisco la lock associata al segmento
	r = pthread_mutex_lock(&cht->mutexs[mutex_idx]);
	if (r != 0) {
		errno = r;
		return -1;
	}

	return 0;
}

int conc_hasht_unlock(conc_hasht_t* cht, void* key) {
	int r;
	unsigned int hash_val;
	unsigned int mutex_idx;

	if (!cht || !key) {
		errno = EINVAL;
		return -1;
	}

	// calcolo il segmento in cui si trova key
	hash_val = (* cht->ht->hash_function)(key) % cht->ht->nbuckets;
	mutex_idx = cht->nsegments == 1 ? 0 : hash_val % cht->nsegments; 

	// rilascio la lock associata al segmento
	r = pthread_mutex_unlock(&cht->mutexs[mutex_idx]);
	if (r != 0) {
		errno = r;
		return -1;
	}
    
	return 0;
}

void* conc_hasht_get_value(conc_hasht_t* cht, void* key) {
	if (!cht || !key) {
		errno = EINVAL;
		return NULL;
	}

	return hasht_get_value(cht->ht,key);
}

int conc_hasht_insert(conc_hasht_t *cht, void* key, void *value) {
	if (!cht || !key) {
		errno = EINVAL;
		return -1;
	}

	return hasht_insert(cht->ht, key, value);
}

int conc_hasht_atomic_insert(conc_hasht_t *cht, void* key, void *value) {
	int r;
	int inserted = 0;
	unsigned int hash_val;
	unsigned int mutex_idx;

	if (!cht || !key) {
		errno = EINVAL;
		return -1;
	}

	// calcolo il segmento in cui si trova key
	hash_val = (* cht->ht->hash_function)(key) % cht->ht->nbuckets;
	mutex_idx = cht->nsegments == 1 ? 0 : hash_val % cht->nsegments; 

	// acquisisco la lock associata al segmento
	r = pthread_mutex_lock(&cht->mutexs[mutex_idx]);
	if (r != 0) {
		errno = r;
		return -1;
	}

	inserted = hasht_insert(cht->ht, key, value);

	// rilascio la lock associata al segmento
	r = pthread_mutex_unlock(&cht->mutexs[mutex_idx]);
	if (r != 0) {
		errno = r;
		return -1;
	}

	return inserted;
}

int conc_hasht_delete(conc_hasht_t *cht, void* key, void (*free_key)(void*), void (*free_value)(void*)) {
	if (!cht || !key) {
		errno = EINVAL;
		return -1;
	}

	return hasht_delete(cht->ht, key, free_key, free_value);
}

int conc_hasht_atomic_delete(conc_hasht_t *cht, void* key, void (*free_key)(void*), void (*free_value)(void*)) {
	int r;
	int deleted = 0;
	unsigned int hash_val;
	unsigned int mutex_idx;

	if (!cht || !key) {
		errno = EINVAL;
		return -1;
	}

	// calcolo il segmento in cui si trova key
	hash_val = (* cht->ht->hash_function)(key) % cht->ht->nbuckets;
	mutex_idx = cht->nsegments == 1 ? 0 : hash_val % cht->nsegments; 

	// acquisisco la lock associata al segmento
	r = pthread_mutex_lock(&cht->mutexs[mutex_idx]);
	if (r != 0) {
		errno = r;
		return -1;
	}

	deleted = hasht_delete(cht->ht, key, free_key, free_value);

	// rilascio la lock associata al segmento
	r = pthread_mutex_unlock(&cht->mutexs[mutex_idx]);
	if (r != 0) {
		errno = r;
		return -1;
	}

	return deleted;
}

void* conc_hasht_delete_and_get(conc_hasht_t *cht, void* key, void (*free_key)(void*)) {
	if (!cht) {
		errno = EINVAL;
		return NULL;
	}
	return hasht_delete_and_get(cht->ht, key, free_key);
}

void* conc_hasht_atomic_delete_and_get(conc_hasht_t *cht, void* key, void (*free_key)(void*)) {
	int r;
	void* value = NULL;
	unsigned int hash_val;
	unsigned int mutex_idx;

	if (!cht || !key) {
		errno = EINVAL;
		return NULL;
	}

	// calcolo il segmento in cui si trova key
	hash_val = (* cht->ht->hash_function)(key) % cht->ht->nbuckets;
	mutex_idx = cht->nsegments == 1 ? 0 : hash_val % cht->nsegments; 

	// acquisisco la lock associata al segmento
	r = pthread_mutex_lock(&cht->mutexs[mutex_idx]);
	if (r != 0) {
		errno = r;
		return NULL;
	}

	value = hasht_delete_and_get(cht->ht, key, free_key);

	// rilascio la lock associata al segmento
	r = pthread_mutex_unlock(&cht->mutexs[mutex_idx]);
	if (r != 0) {
		errno = r;
		return NULL;
	}

	return value;
}