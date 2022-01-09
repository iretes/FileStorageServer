#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <util.h>

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