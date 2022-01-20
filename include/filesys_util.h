/**
 * @file     filesys_util.h
 * @brief    Interfaccia delle routine di utilità per l'interazione con il file system
 */

#ifndef FILESYS_UTIL_H
#define FILESYS_UTIL_H

/*
* Numero massimo di file con lo stesso nome e lo stesso path ammissibile
* (i file con stesso nome vengono rinominati aggiungendo prima dell'estensione un numero progressivo tra parentesi tonde)
*/
#define MAX_FILE_VERSION 9999

// prototipo della funzione di libreria
char* realpath(const char* restrict path, char* restrict resolved_path);

/**
 * @function           get_absolute_path()
 * @brief              Alloca memoria e ritorna una stringa che rappresenta il path assoluto del file.
 * 
 * @param file_name    Il nome del file
 * 
 * @return             Una stringa che rappresenta il path assoluto del file in caso di successo,
 *                     NULL in caso di fallimento ed errno settato a segnalare l'errore.
 *                     In caso di fallimento errno può assumere i seguenti valori:
 *                     EINVAL se file_name è @c NULL o la sua lunghezza è 0 o > PATH_MAX-1.
 * @note               Può fallire e settare errno se si verificano gli errori specificati da calloc() e realpath().
 */
char* get_absolute_path(const char* file_name);

#endif /* FILESYS_UTIL_H */