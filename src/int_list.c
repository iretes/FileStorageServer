/**
 * @file    int_list.c
 * @brief   Implementazione dell'interfaccia della linked list di interi
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <list.h>
#include <int_list.h>

/**
 * @function int_list_compare_elem()
 * @brief   Confronta due interi (elementi della lista)
 * 
 * @param a primo elemento da confrontare
 * @param b secondo elemento da confrontare
 *
 * @return  1 se gli interi sono uguali, 0 altrimenti
 */
static int int_list_compare_elem(void* a, void* b) {
    return ((*(int*)a - *(int*)b) == 0);
}

int_list_t* int_list_create() {
    int_list_t* int_l = malloc(sizeof(int_list_t));
    if (!int_l)
        return NULL;

    int_l->list = list_create(int_list_compare_elem, free);
    if (!int_l->list)
        return NULL;

    return int_l;
}

void int_list_destroy(int_list_t* int_list) {
    if (!int_list)
        return;

    list_destroy(int_list->list, 1);
    free(int_list);
    int_list = NULL;
}

int int_list_tail_insert(int_list_t* int_list, int data) {
    if (!int_list) {
        errno = EINVAL;
        return -1;
    }

    int* data_ptr = malloc(sizeof(int));
    if (!data_ptr)
        return -1;

    *data_ptr = data;
    if (list_tail_insert(int_list->list, data_ptr) == -1) {
        free(data_ptr);
        return -1;
    }

    return 0;
}

int int_list_tail_remove(int_list_t* int_list, int* data) {
    if (!int_list) {
        errno = EINVAL;
        return -1;
    }

    int* data_ptr = list_tail_remove(int_list->list);
    if (!data_ptr)
        return -1;

    *data = *data_ptr;
    free(data_ptr);

    return 0;
}

int int_list_head_insert(int_list_t* int_list, int data) {
    if (!int_list) {
        errno = EINVAL;
        return -1;
    }

    int* data_ptr = malloc(sizeof(int));
    if (!data_ptr)
        return -1;

    *data_ptr = data;
    if (list_head_insert(int_list->list, data_ptr) == -1) {
        free(data_ptr);
        return -1;
    }

    return 0;
}

int int_list_head_remove(int_list_t* int_list, int* data) {
    if (!int_list) {
        errno = EINVAL;
        return -1;
    }

    int* data_ptr = list_head_remove(int_list->list);
    if (!data_ptr)
        return -1;

    *data = *data_ptr;
    free(data_ptr);

    return 0;
}

int int_list_remove(int_list_t* int_list, int data) {
    if (!int_list) {
        errno = EINVAL;
        return -1;
    }

    return list_remove(int_list->list, &data);
}

int int_list_contains(int_list_t* int_list, int data) {
    if (!int_list) {
        errno = EINVAL;
        return -1;
    }

    return list_contains(int_list->list, &data);
}

int int_list_is_empty(int_list_t* int_list) {
    if (!int_list) {
        errno = EINVAL;
        return -1;
    }

    return list_is_empty(int_list->list);
}

size_t int_list_get_length(int_list_t* int_list) {
    if (!int_list) {
        errno = EINVAL;
        return 0;
    }

    return list_get_length(int_list->list); 
}

void int_list_print(int_list_t* int_list) {
    if (!int_list) {
        printf("List is NULL\n");
        return;
    }

    if (!int_list->list)
        printf("List is empty\n");

    int* data;
    list_for_each(int_list->list, data) {
        printf("%d ", *(int*)data);
    }
    printf("\n");
}

int int_list_concatenate(int_list_t* int_list1, int_list_t* int_list2) {
    if (!int_list1 || !int_list2) {
        errno = EINVAL;
        return -1;
    }

    int* data = NULL;
    list_for_each(int_list2->list, data) {
        if (int_list_tail_insert(int_list1, *data) == -1)
            return -1;
    }

    return 0;
}

int_list_t* int_list_cpy(int_list_t* int_list) {
    if (!int_list) {
        errno = EINVAL;
        return NULL;
    }

    int_list_t* int_list_cpy = int_list_create();
    if (!int_list_cpy)
        return NULL;

    int* data;
    list_for_each(int_list->list, data) {
       if (int_list_tail_insert(int_list_cpy, *data) == -1)
        return NULL;
    }

    return int_list_cpy;
}