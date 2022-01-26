/**
 * @file     list.c
 * @brief    File di implementazione dell'interfaccia della linked list.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <list.h>

list_t* list_create(int (*compare)(void *, void*), void (*free_data)(void*)) {
	if (!compare || !free_data) {
		errno = EINVAL;
		return NULL;
	}

	list_t* l = malloc(sizeof(list_t));
	if (!l) 
		return NULL;

	l->head = l->tail = NULL;
	l->compare = compare;
	l->free_data = free_data;
	l->len = 0;
	return l;
}

void list_destroy(list_t* list, int free_data) {
	if (!list) 
		return;

	while (list->head != NULL) {
		node_t* p = (node_t*)list->head;
		if (free_data && p->data) {
			list->free_data(p->data);
		}
		list->head = list->head->next;
		free(p);
	}

	free(list);
}

int list_tail_insert(list_t* list, void* data) {
	if (!list || !data) { 
		errno = EINVAL; 
		return -1;
	}

	node_t* n = malloc(sizeof(node_t));
	if (!n) 
		return -1;

	n->data = data; 
	n->next = NULL;

	if (!list->head) {
		list->head = list->tail = n;
	}
	else {
		list->tail->next = n;
		list->tail = n;
	}

	(list->len) ++;
	return 0;
}

void* list_tail_remove(list_t* list) {
	if (!list) {
		errno = EINVAL;
		return NULL;
	}

	node_t* curr = list->head, *prev = NULL;
	if (!curr)
		return NULL;

	while (curr->next != NULL) {
		prev = curr;
		curr = curr->next;
	}

	void* data = curr->data;
	if (!prev) {
		list->head = list->tail = NULL;
	}
	else {
		prev->next = curr->next;
		list->tail = prev;
	}

	free(curr);
	(list->len)--;
	return data;
}

int list_head_insert(list_t* list, void* data) {
	if (!list || !data) { 
		errno = EINVAL; 
		return -1;
	}

	node_t* n = malloc(sizeof(node_t));
	if (!n) 
		return -1;

	n->data = data; 
	n->next = NULL;

	if (!list->head) {
		list->head = list->tail = n;
	}
	else {
		n->next = list->head;
		list->head = n;
	}

	(list->len) ++;
	return 0;
}

void* list_head_remove(list_t* list) {
	if (!list) { 
		errno = EINVAL; 
		return NULL;
	}

	if (!list->head)
		return NULL;

	node_t *n = (node_t *)list->head;
	void *data = (list->head)->data;

	list->head = list->head->next;
	if (!list->head)
		list->tail = NULL;

	(list->len) --;

	free((void*)n);
	return data;
}

void* list_remove_and_get(list_t* list, void* data) {
	if (!list || !data) { 
		errno = EINVAL; 
		return NULL;
	}

	node_t* curr = list->head, *prev = NULL;
	while (curr != NULL && !(list->compare(curr->data, data))) {
		prev = curr;
		curr = curr->next;
	}
	if (!curr)
		return NULL;

	void* res = curr->data;
	if (!prev) {
		list->head = list->head->next;
		if (list->head == NULL)
			list->tail = NULL;
	}
	else if (!curr->next) {
		prev->next = NULL;
		list->tail = prev;
	}
	else {
		prev->next = curr->next;
	}

	free(curr);
	(list->len)--;
	return res;
}

int list_remove(list_t* list, void* data) {
	if (!list|| !data) { 
		errno = EINVAL; 
		return -1;
	}

	node_t* curr = list->head, *prev = NULL;
	while (curr != NULL && !(list->compare(curr->data, data))) {
		prev = curr;
		curr = curr->next;
	}
	if (!curr)
		return -1;

	if (!prev) {
		list->head = list->head->next;
		if (!list->head)
			list->tail = NULL;
	}
	else if (!curr->next) {
		prev->next = NULL;
		list->tail = prev;
	}
	else {
		prev->next = curr->next;
	}

	list->free_data(curr->data);
	free(curr);
	(list->len)--;
	return 0;
}

int list_contains(list_t* list, void* data) {
	if (!list || !data) { 
		errno = EINVAL; 
		return -1;
	}

	node_t* curr = list->head;
	while (curr != NULL && !list->compare(curr->data, data)) {
		curr = curr->next;
	}

	if (curr)
		return 1;

	return 0;
}

int list_is_empty(list_t* list) {
	if (!list) {
		errno = EINVAL;
		return -1;
	}
	return (list->len == 0);
}

size_t list_get_length(list_t* list) {
	if (!list) { 
		errno = EINVAL; 
		return 0;
	}
	return list->len;
}

void list_print(list_t* list) {
	if (!list) {
		printf("List is NULL\n");
		return;
	}
	if (!list->head) {
		printf("List is EMPTY\n");
		return;
	}

	node_t* curr = list->head;
	while (curr != NULL) {
		printf("%p\n", curr->data);
		curr = curr->next;
	}
}

int list_reverse(list_t* list) {
	if (!list) {
		errno = EINVAL;
		return -1;
	}

	node_t* prev = NULL;
	node_t* curr = list->head;
	node_t* next = NULL;
	list->tail = list->head;
	while (curr != NULL) {
		next = curr->next;
		curr->next = prev;
		prev = curr;
		curr = next;
	}
	if (prev)
		list->head = prev;

	return 0;
}