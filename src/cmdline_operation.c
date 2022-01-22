/**
* @file     cmdline_operation.c
* @brief    Implementazione dell'oggetto che rappresenta un'operazione specificata della linea di comando del client
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include <cmdline_operation.h>
#include <list.h>

cmdline_operation_t* cmdline_operation_create(char operation) {
	if (operation != 'w' && operation != 'W' && 
		operation != 'a' && operation != 'r' &&
		operation != 'R' && operation != 'l' && 
		operation != 'u' && operation != 'c') {
		errno = EINVAL;
		return NULL;
	}
	cmdline_operation_t* cmdline_operation = malloc(sizeof(cmdline_operation_t));
	if (!cmdline_operation) {
		return NULL;
	}
	cmdline_operation->operation = operation;
	cmdline_operation->dirname_in = NULL;
	cmdline_operation->dirname_out = NULL;
	cmdline_operation->source_file = NULL;
	cmdline_operation->files = NULL;
	cmdline_operation->time = -1;
	cmdline_operation->n = 0;
	return cmdline_operation;
}

void cmdline_operation_destroy(cmdline_operation_t* cmdline_operation) {
	if (!cmdline_operation)
		return;
	if (cmdline_operation->files)
		list_destroy(cmdline_operation->files, LIST_FREE_DATA);
	if (cmdline_operation->dirname_in)
		free(cmdline_operation->dirname_in);
	if (cmdline_operation->dirname_out)
		free(cmdline_operation->dirname_out);
	if (cmdline_operation->source_file)
		free(cmdline_operation->source_file);
	free(cmdline_operation);
}

int cmdline_operation_cmp(void* a, void* b) {
	return (a == b);
}

void cmdline_operation_print(cmdline_operation_t* cmdline_operation) {
	if (!cmdline_operation) {
		printf("NULL\n");
		return;
	}
	printf("-%c", cmdline_operation->operation);
	if (cmdline_operation->files) {
		char* file = NULL;
		bool first = true;
		list_for_each(cmdline_operation->files, file) {
			if (first)
				printf(" %s", file);
			else
				printf(",%s", file);
			first = false;
		}
	}
	if (cmdline_operation->dirname_in)
		printf(" dirname=%s", cmdline_operation->dirname_in);
	if ((cmdline_operation->operation == 'r' || cmdline_operation->operation == 'R') && 
		(cmdline_operation->dirname_out))
		printf(" -d %s", cmdline_operation->dirname_out); 
	if ((cmdline_operation->operation == 'w' || cmdline_operation->operation == 'W' || 
		 cmdline_operation->operation == 'a') && (cmdline_operation->dirname_out))
		printf(" -D %s", cmdline_operation->dirname_out);
	if (cmdline_operation->time != -1)
		printf(" -t %ld", cmdline_operation->time);
	if (cmdline_operation->operation == 'R' || cmdline_operation->operation == 'w')
		printf(" n=%d", cmdline_operation->n);
	printf("\n");
}