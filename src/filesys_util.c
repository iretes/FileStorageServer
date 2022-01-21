/**
 * @file    filesys_util.c
 * @brief   Implementazione delle routine di utilità per l'interazione con il file system
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <filesys_util.h>

/**
 * @function           build_path()
 * @brief              Concatena a dirname filename separandoli, se necessario, con '/'.
 * 
 * @param dir_name     Il path della directory
 * @param file_name    Il nome del file
 * 
 * @return             Una stringa che rappresenta un path in caso di successo,
 *                     NULL in caso di fallimento ed errno settato ad indicare l'errore.
 *                     In caso di fallimento errno può assumere i seguenti valori:
 *                     EINVAL          se dir_name o file_name sono @c NULL o hanno lunghezza 0
 *                     ENAMETOOLONG    se il path complessivo è troppo lungo
 * @note               Può fallire e settare errno se si verificano gli errori specificati da calloc().
 */
char* const build_path(const char* dir_name, const char* file_name) {
	if (!file_name || strlen(file_name) == 0 || !dir_name || strlen(dir_name) == 0) {
		errno = EINVAL;
		return NULL;
	}

	// calcolo la dimensione del path risultante e lo alloco
	char* file_path = NULL;
	size_t dir_name_size = strlen(dir_name);
	size_t file_path_size = 0;
	file_path_size = strlen(dir_name) + strlen(file_name) + 2;
	if (file_path_size > PATH_MAX) {
		errno = ENAMETOOLONG;
		return NULL;
	}
	file_path = calloc(file_path_size, sizeof(char));
	if (!file_path)
		return NULL;

	// concateno a dir_name file_name separandoli necessario da '/'
	strcpy(file_path, dir_name);
	if (file_path[dir_name_size-1] != '/')
		strncat(file_path, "/", file_path_size);
	strncat(file_path, file_name, file_path_size);

	return file_path;
}

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

/**
 * @function   get_num_digits()
 * @brief      Ritorna il numero di cifre di un intero
 * 
 * @param n    L'intero di cui si vuole conoscere il numero di cifre
 * 
 * @return     Il numero di cifre di n in caso di successo, 
 *             -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *             In caso di fallimento errno può assumere i seguenti valori:
 *             EINVAL se n < 0
 */
static int get_decimal_num_digits(int n) {
	if (n < 0) {
		errno = EINVAL;
		return -1;
	}
	int d = 1;
	while (n > 9) {
		n /= 10;
		d++;
	}
	return d;
}

char* build_notexisting_path(const char* dir_name, const char* file_name) {
	if (file_name == NULL || strlen(file_name) == 0 || 
		dir_name == NULL || strlen(dir_name) == 0) {
		errno = EINVAL;
		return NULL;
	}

	// ottengo il path assoluto di dir_name
	char* abs_dir_name = get_absolute_path(dir_name);
	if (abs_dir_name == NULL)
		return NULL;

	// costruisco un path concatenando dir_name a file_name separandoli con '/'
	char* file_path = build_path(abs_dir_name, file_name);
	free(abs_dir_name);
	if (file_path == NULL)
		return NULL;

	// apro il file dal path costruito
	int fd;
	fd = open(file_path, O_CREAT|O_EXCL, S_IRWXU);

	// se open non ha fallito il file non esiste
	if (fd != -1) {
		if (close (fd) == -1) {
			free(file_path);
			return NULL;
		}
		return file_path;
	}
	// se l'errore è != EEXIST esco settando errno
	else if (errno != EEXIST) {
		free(file_path);
		return NULL;
	}
	// altrimenti il file esiste già

	/* calcolo il massimo numero di cifre necessarie 
	   per rappresentare MAX_FILE_VERSION in base 10 */
	int max_digit_version = get_decimal_num_digits(MAX_FILE_VERSION);
	if (max_digit_version == -1) {
		free(file_path);
		errno = EFAULT;
		return NULL;
	}
	// calcolo la size del nuovo path da costruire e lo alloco
	size_t new_path_size = strlen(file_path) + 3 + max_digit_version;
	if (new_path_size > PATH_MAX) {
		free(file_path);
		errno = ENAMETOOLONG;
		return NULL;
	}
	char* new_file_path = calloc(new_path_size, sizeof(char));
	if (new_file_path == NULL) {
		free(file_path);
		return 0;
	}

	// indice del numero progressivo da aggiungere al path
	int version = 2;

	do {
		memset(new_file_path, 0, new_path_size);
		// alloco la stringa da concatenare al nome del file
		char* version_str = calloc(3 + max_digit_version, sizeof(char));
		if (version_str == NULL) {
			free(file_path);
			free(new_file_path);
			return NULL;
		}
		// stabilisco se il file presenta un'estensione
		char* name = strrchr(file_path, '.');
		char* ext = NULL;
		int dotpos;
		// in tal caso la copio
		if (name != NULL && name != file_path + (strlen(file_path) - 1)) {
			ext = calloc(strlen(file_path) + 1, sizeof(char));
			strcpy(ext, name);
			dotpos = name - file_path;
		}
		else
			dotpos = strlen(file_path);
		// concateno al nome del file il numero progressivo tra parentesi
		sprintf(version_str, "(%d)", version);
		strcpy(new_file_path, file_path);
		strcpy(new_file_path + dotpos, version_str);
		// se il file aveva un'estensione la concateno
		if (ext != NULL) {
			strcat(new_file_path, ext);
			free(ext);
		}
		free(version_str);
		version ++;
		errno = 0;
		/* se il file dal nuovo path esiste continuo a generarne nuovi 
		   fino a che si verifica un errore o raggiungo il massimo numero progressivo */
	} while((fd = open(new_file_path, O_CREAT | O_EXCL, S_IRWXU)) == -1 && 
			 errno == EEXIST && 
			 version <= MAX_FILE_VERSION);
	int errnosv = errno;

	// se tutti i file con i numeri progressivi esistono esco settando errno
	if (fd == -1 && version > MAX_FILE_VERSION) {
		free(file_path);
		free(new_file_path);
		errno = EEXIST;
		return NULL;
	}
	// se si è verificato un errore nella open esco settando errno
	if (fd == -1) {
		free(file_path);
		free(new_file_path);
		errno = errnosv;
		return NULL;
	}
	// se si verifica un errore nella close esco settando errno
	if (close(fd) == -1) {
		free(file_path);
		free(new_file_path);
		return NULL;
	}

	errno = 0;
	free(file_path);
	return new_file_path;
}

char* get_basename(const char* path) {
	if (!path || strlen(path) == 0) {
		errno = EINVAL;
		return NULL;
	}
	int len = strlen(path);
	char* path_cpy = calloc(len + 1, sizeof(char));
	if (!path_cpy)
		return NULL;

	// individuo la posizione dell'ulitmo '/'
	char* basename = strrchr(path, '/');
	if (!basename || (basename-path) == len-1)
		strcpy(path_cpy, path);
	else
		strcpy(path_cpy, basename + 1);

	// se il nome del file termina con '/' tolgo tale carattere
	len = strlen(path_cpy);
	if (path_cpy[len-1] == '/')
		path_cpy[len-1] = '\0';

	return path_cpy;
}

int mkdirr(char const *path) {
	if (path == NULL || strlen(path) == 0) {
		errno = EINVAL;
		return -1;
	}
	const size_t path_len = strlen(path);
	char path_cpy[PATH_MAX];
	char *chr; 
	errno = 0;

	// copio la stringa
	if (path_len > sizeof(path_cpy)-1) {
		errno = ENAMETOOLONG;
		return -1; 
	}
	strcpy(path_cpy, path);

	// scorro i caratteri della stringa 
	for (chr = path_cpy + 1; *chr; chr++) {
		if (*chr == '/') {
			// tronco temporaneamente la stringa
			*chr = '\0';

			// creo la directory
			if (mkdir(path_cpy, S_IRWXU) != 0) {
				// se esiste già proseguo
				if (errno != EEXIST)
					return -1; 
			}
			// ripristino '/'
			*chr = '/';
		}
	}

	if (mkdir(path_cpy, S_IRWXU) != 0) {
		if (errno != EEXIST)
			return -1; 
	}

	return 0;
}

int is_dot(const char path[]) {
	if (!path) {
		errno = EINVAL;
		return -1;
	}
	int l = strlen(path);
	if (l > 0 && path[l-1] == '.') 
		return 1;
	return 0;
}