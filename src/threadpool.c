/**
 * @file        threadpool.c
 * @brief       File di implementazione dell'interfaccia del threadpool.
 */

#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>

#include <threadpool.h>
#include <util.h>

/**
 * @function    workerpool_thread
 * @brief       Funzione eseguita dal thread worker che appartiene al pool
 */
static void *workerpool_thread(void *arguments) {
	worker_args_t* args = (worker_args_t *)arguments;
	threadpool_t *pool = args->pool;
	int myid = args->id;

	taskfun_node_t *taskfun = NULL; // task generico

	int r;
	LOCK_DO(&(pool->lock), r, goto workerpool_thread_exit);
	for (;;) {

		// in attesa di un messaggio, controllo spurious wakeups
		while ((pool->count == 0) && (!pool->exiting)) {
			WAIT(&(pool->cond), &(pool->lock), r);
			if (r != 0) {
				UNLOCK_DO(&(pool->lock), r, goto workerpool_thread_exit);
				goto workerpool_thread_exit;
			}
		}

		if (pool->exiting && !pool->count) // termino
			break; 

		// estraggo un task dalla testa della lista
		assert(pool->lhead != NULL);
		taskfun = pool->lhead;
		pool->lhead = pool->lhead->next;
		if (pool->lhead == NULL)
			pool->ltail = NULL;

		pool->count--;
		pool->taskonthefly++;
		UNLOCK_DO(&(pool->lock), r, goto workerpool_thread_exit);

		// eseguo la funzione 
		(*(taskfun->fun))(taskfun->arg, myid);
		free(taskfun);
		taskfun = NULL;

		LOCK_DO(&(pool->lock), r, goto workerpool_thread_exit);
		pool->taskonthefly--;
	}
	UNLOCK_DO(&(pool->lock), r, goto workerpool_thread_exit);

workerpool_thread_exit:
	if (taskfun) {
		if (taskfun->arg)
			free(taskfun->arg);
		free(taskfun);
	}
	free(args);
	return NULL;
}

void free_task_list(taskfun_node_t *node) {
	while (node != NULL) {
		taskfun_node_t *tmp = node;
		node = node->next;
		free(tmp);
	}
}

void free_pool_resources(threadpool_t *pool) {
	if (!pool->threads)
		return;
	free(pool->threads);
	free_task_list(pool->lhead);
	pthread_mutex_destroy(&(pool->lock));
	pthread_cond_destroy(&(pool->cond));
	free(pool);
}

threadpool_t* threadpool_create(size_t numthreads, size_t pending_size) {
	int r, errnosv;
	if (numthreads == 0 || pending_size == 0) {
		errno = EINVAL;
		return NULL;
	}

	threadpool_t *pool = malloc(sizeof(threadpool_t));
	if (!pool)
		return NULL;

	// condizioni iniziali
	pool->numthreads   = 0;
	pool->taskonthefly = 0;
	pool->queue_size = pending_size;
	pool->count = 0;
	pool->exiting = false;

	// alloco i thread
	pool->threads = malloc(sizeof(pthread_t)*numthreads);
	if (!pool->threads) {
		free(pool);
		return NULL;
	}

	// condizioni iniziali della lista di task
	pool->lhead = NULL;
	pool->ltail = NULL;

	r = pthread_mutex_init(&(pool->lock), NULL);
	if (r != 0) {
		free(pool->threads);
		free(pool);
		errno = r;
		return NULL;
	}

	r = pthread_cond_init(&(pool->cond), NULL);
	if (r != 0)  {
		free(pool->threads);
		free(pool);
		pthread_mutex_destroy(&(pool->lock));
		errno = r;
		return NULL;
	}

	for (int i = 0; i < numthreads; i++) {
		worker_args_t* worker_args = malloc(sizeof(worker_args_t));
		if (!worker_args) {
			errnosv = errno;
			threadpool_destroy(pool);
			errno = errnosv;
			return NULL;
		}
		worker_args->id = i+1;
		worker_args->pool = pool;
		r = pthread_create(&(pool->threads[i]), NULL, workerpool_thread, (void*)worker_args);
		if (r != 0) {
			free(worker_args);
			threadpool_destroy(pool);
			errno = r;
			return NULL;
		}
		pool->numthreads++;
	}
	return pool;
}

int threadpool_destroy(threadpool_t *pool) {    
	if (!pool) {
		errno = EINVAL;
		return -1;
	}
	int r;
	LOCK_DO(&(pool->lock), r, errno = r; return -1);

	pool->exiting = true;

	BCAST(&(pool->cond), r);
	if (r != 0) {
		UNLOCK_DO(&(pool->lock), r, errno = r; return -1);
		errno = r;
		return -1;
	}

	UNLOCK_DO(&(pool->lock), r, errno = r; return -1);

	for (int i = 0; i < pool->numthreads; i++) {
		r = pthread_join(pool->threads[i], NULL);
		if (r != 0)
			errno = r;
	}

	free_pool_resources(pool);

	if (r != 0)
		return -1;
	return 0;
}

int threadpool_add(threadpool_t *pool, void (*f)(void *, int), void *arg) {
	if (!pool || !f) {
		errno = EINVAL;
		return -1;
	}

	int r, errnosv;
	LOCK_DO(&(pool->lock), r, errno = r; return -1);

	// coda piena o in fase di uscita
	if (pool->count >= pool->queue_size || pool->exiting) {
		UNLOCK_DO(&(pool->lock), r, errno = r; return -1);
		return 1; // esco con valore "coda piena"
	}

	taskfun_node_t* task_node = malloc(sizeof(taskfun_node_t));
	if (!task_node) {
		errnosv = errno;
		UNLOCK_DO(&(pool->lock), r, errno = r; return -1);
		errno = errnosv;
		return -1;
	}

	// inserisco il task in coda alla lista
	task_node->fun = f;
	task_node->arg = arg;
	task_node->next = NULL;
	if (!pool->lhead) {
		pool->lhead = task_node;
		pool->ltail = task_node;
	}
	else {
		pool->ltail->next = task_node;
		pool->ltail = task_node;
	}
	pool->count++;

	SIGNAL(&(pool->cond), r);
	if (r != 0) {
		free(task_node);
		UNLOCK_DO(&(pool->lock), r, errno = r; return -1);
		errno = r;
		return -1;
	}

	UNLOCK_DO(&(pool->lock), r, errno = r; return -1);
	return 0;
}