/**
 * @file               filesys_util.h
 * @brief              Interfaccia delle routine di utilità per l'interazione con il file system.
 */

#ifndef FILESYS_UTIL_H
#define FILESYS_UTIL_H

/*
* Numero massimo di file con lo stesso nome e lo stesso path ammissibile
* (i file con stesso nome vengono rinominati aggiungendo prima dell'estensione un numero progressivo tra parentesi tonde)
*/
#define MAX_FILE_VERSION 9999

/* prototipo della funzione di libreria */
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

/**
 * @function           get_basename()
 * @brief              Restituisce il nome del file.
 * @warning            La stringa ritornata dovrà essere deallocata dal chiamante.
 *                     Può ritornare "." o "..".
 * 
 * @param path         Path del file
 * 
 * @return             Una stringa che rappresenta il nome del file in casod di successo, 
 *                     NULL in caso di fallimento ed errno settato a segnalare l'errore.
 *                     In caso di fallimento errno può assumere i seguenti valori:
 *                     EINVAL se path è @c NULL
 * @note               Può fallire e settare errno se si verificano gli errori specificati da calloc().
 */
char* get_basename(const char* path);

/**
 * @function           build_notexisting_pathname()
 * @brief              Concatena dirname a pathname separandoli, se necessario, con '/' e se esiste già nel file system un 
 *                     file con il path così costruito aggiunge prima dell'estensione, o alla file del nome se l'estensione 
 *                     non è presente, un numero progressivo tra parentesi tonde , fino a individuare un path non esistente 
 *                     o fino ad aver raggiunto MAX_FILE_VERSION (ad esempio se dir_name vale "./dir" e file_name vale 
 *                     "file.txt" ed esistono già i file "./dir/file.txt"  e "./dir/file(2).txt" restituisce 
 *                     ./dir/file(3).txt)
 * 
 * @param dir_name     Il path della directory
 * @param file_name    Il nome del file
 * 
 * @return             Una stringa che rappresenta un path in caso di successo,
 *                     NULL in caso di fallimento ed errno settato ad indicare l'errore.
 *                     In caso di fallimento errno può assumere i seguenti valori:
 *                     EINVAL se dir_name o file_name sono @c NULL o hanno lunghezza 0
 *                     ENAMETOOLONG se il path risultante è troppo lungo
 *                     EFAULT se si sono verifiati errori durante la costruzione del path
 * @note               Può fallire e settare errno se si verificano gli errori specificati da get_absolute_path(), 
 *                     calloc(), open(), close().
 */
char* build_notexisting_path(const char* dir_name, const char* file_name);

/**
 * @function           mkdirr()
 * @brief              Crea la directory path ricorsivamente (e eventuali directory padre) se non esiste.
 * 
 * @param path         Il path della directory da creare
 * 
 * @return             0 in caso di successo, -1 in caso di fallimento ed errno settato ad indicare l'errore.
 *                     In caso di fallimento errno può assumere i seguenti valori:
 *                     EINVAL se path è @c NULL o la sua lunghezza è 0
 *                     ENAMETOOLONG se path è troppo lungo
 * @note               Può fallire e settare errno se si verificano gli errori specificati da mkdir().
 */
int mkdirr(char const *path);

/**
 * @function           is_dot()
 * @brief              Consente di stabilire se path è uguale a "." o "..".
 * 
 * @param path         Il path del file
 * 
 * @return             1 in caso di successo e se path è uguale a "." p "..", 
 *                     0 in caso di successo e se path non è uguale a "." p "..", 
 *                     -1 in caso di fallimento con errno settato ad indicare l'errore.
 *                     In caso di fallimento errno può assumere i seguenti valori:
 *                     EINVAL se path è @c NULL
 */
int is_dot(const char path[]);

#endif /* FILESYS_UTIL_H */