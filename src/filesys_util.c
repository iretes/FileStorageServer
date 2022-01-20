/**
 * @file    filesys_util.c
 * @brief   Implementazione delle routine di utilità per l'interazione con il file system
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <filesys_util.h>

char* get_absolute_path(const char* file_name) {
	if (!file_name || strlen(file_name) == 0 || strlen(file_name) > (PATH_MAX-1)) {
		errno = EINVAL;
		return NULL;
	}
	char* real_path = calloc(PATH_MAX, sizeof(char));
	if (!real_path) 
		return NULL;

	// se non è un path assoluto lo ricavo con realpath
	if (file_name[0] != '/') {
		if (realpath(file_name, real_path) == NULL) {
			free(real_path);
			return NULL;
		}
	}
	// altrimenti copio file_name
	else {
		strncpy(real_path, file_name, PATH_MAX-1);
	}
	return real_path;
}