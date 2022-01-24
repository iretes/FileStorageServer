/**
 * @file     util.c
 * @brief    Implementazione di funzioni di utilità.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include <util.h>

int int_cmp(void* a, void* b) {
	return ((*(int*)a - *(int*)b) == 0);
}

int readn(long fd, void *buf, size_t size) {
	size_t left = size;
	int r;
	char *bufptr = (char*)buf;
	while (left > 0) {
		if ((r = read((int)fd, bufptr, left)) == -1) {
			if (errno == EINTR) // proseguo in caso di ricezione di interruzione 
				continue;
			return -1;
		}
		if (r == 0) // EOF
			return 0;
		left -= r;
		bufptr += r;
	}
	return size;
}

int writen(long fd, void *buf, size_t size) {
	size_t left = size;
	int r;
	char *bufptr = (char*)buf;
	while (left > 0) {
		if ((r = write((int)fd ,bufptr,left)) == -1) {
			if (errno == EINTR) // proseguo in caso di ricezione di interruzione 
				continue;
			return -1;
		}
		if (r == 0) // EOF
			return 0;
		left -= r;
		bufptr += r;
	}
	return 1;
}

int is_number(const char* s, long* n) {
	if (s == NULL || strlen(s) == 0) {
		errno = EINVAL;
		return 1;
	}
	char* e = NULL;
	errno = 0;
	long val = strtol(s, &e, 10);
	if (errno == ERANGE) // overflow / underflow
		return 2;
	if (e != NULL) {
		for (int i = 0, len = strlen(e); i < len; i ++)
			if (!isspace(e[i]))
				return 1;
		*n = val;
		return 0;
	}
	return 1; // non e' un numero
}

int millisleep(long ms) {
	int err;
	struct timespec towait = {0, 0}, remaining = {0, 0};

	if (ms <= 0) {
		errno = EINVAL;
		return -1;
	}

	towait.tv_sec = ms / 1000;
	towait.tv_nsec = (ms % 1000) * 1000000;

	do {
		err = nanosleep(&towait, &remaining);
		towait = remaining;
	} while (err != 0 && errno == EINTR); // proseguo in caso di ricezione di interruzione

	return err;
}