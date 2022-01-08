/**
 * @file    util.h
 * @brief   Interfaccia delle routine e macro di utilità.
 */

#ifndef UTIL_H
#define UTIL_H

#include <errno.h>

/**
 * @def     EXTF
 * @brief   Esce dal processo con valore EXIT_FAILURE.
 */
#define EXTF exit(EXIT_FAILURE)

/**
 * @def     PERRORSTR()
 * @brief   Stampa su stderr la descrizione di errno.
 * 
 * @param r Valore dell'errore
 */
#define PERRORSTR(r) \
    do { \
        int err = r; \
        char err_s[512] = {0}; \
        strerror_r(err, err_s, 512); \
        if (strstr(err_s, "Unknown error") == NULL)  \
            fprintf(stderr, "ERR: %s:%s():%d: %s\n", __FILE__, __func__, __LINE__, err_s); \
        else \
            fprintf(stderr, "ERR: %s:%s():%d: %d\n", __FILE__, __func__, __LINE__, err); \
        fflush(stderr); \
    } while(0);

/**
 * @def     PERRNO()
 * @brief   Stampa su stderr il valore dell'errore.
 * 
 * @param r Valore dell'errore
 */
#define PERRNO(r) \
    do { \
        fprintf(stderr, "ERR: %s:%s():%d: %d\n", __FILE__, __func__, __LINE__, errno); \
        fflush(stderr); \
    } while(0);

/**
 * @def     ERRNOSET_DO()
 * @brief   Esegue X, salva il valore che ritorna in r e se errno viene settato da X stampa l'errore ed esegue y.
 * 
 * @param X Istruzione da eseguire
 * @param r Dove salvare il valore ritornato da X
 * @param y Istruzione da eseguire se X setta errno
 */
#define ERRNOSET_DO(X, r, y) \
    do { \
    errno = 0; \
    r = (X); \
    if (errno != 0) { \
        PERRNO(errno) \
        { y; } \
    } \
    } while(0);

/**
 * @def     ERRNOSET()
 * @brief   Esegue X e salva il valore che ritorna in r, se errno viene settato da X stampa l'errore.
 * 
 * @param X Istruzione da eseguire
 * @param r Dove salvare il valore ritornato da X
 */
#define ERRNOSET(X, r) \
    do { \
    errno = 0; \
    r = (X); \
    if (errno != 0) { \
        PERRNO(errno) \
    } \
    } while(0);

/**
 * @def     LOCK_DO()
 * @brief   Esegue pthread_mutex_lock() sulla mutex l, salva il valore ritornato in r, e in caso di errore stampa l'errore 
 *          ed esegue y.
 * 
 * @param l Riferimento alla mutex
 * @param r Dove salvare il valore ritornato da pthread_mutex_lock()
 * @param y Istruzione da eseguire in caso di errore
 */
#define LOCK_DO(l, r, y) \
    do { \
    if ((r = pthread_mutex_lock(l)) != 0) { \
        PERRORSTR(r); \
        { y; } \
    } \
    } while(0);

/**
 * @def     LOCK()
 * @brief   Esegue pthread_mutex_lock() sulla mutex l, salva il valore ritornato in r, e in caso di errore stampa l'errore.
 * 
 * @param l Riferimento alla mutex
 * @param r Dove salvare il valore ritornato da pthread_mutex_lock()
 */
#define LOCK(l, r) \
    do { \
    if ((r = pthread_mutex_lock(l)) != 0) { \
        PERRORSTR(r); \
    }\
    } while(0);

/**
 * @def     UNLOCK_DO()
 * @brief   Esegue pthread_mutex_unlock() sulla mutex l, salva il valore ritornato in r, e in caso di errore stampa 
 *          l'errore ed esegue y.
 * 
 * @param l Riferimento alla mutex
 * @param r Dove salvare il valore ritornato da pthread_mutex_unlock()
 * @param y Istruzione da eseguire in caso di errore
 */
#define UNLOCK_DO(l, r, y) \
    do { \
    if ((r = pthread_mutex_unlock(l)) != 0) { \
        PERRORSTR(r); \
        { y; } \
    }\
    } while(0);

/**
 * @def     UNLOCK()
 * @brief   Esegue pthread_mutex_unlock() sulla mutex l, salva il valore ritornato in r, e in caso di errore stampa l'errore.
 * 
 * @param l Riferimento alla mutex
 * @param r Dove salvare il valore ritornato da pthread_mutex_unlock()
 */
#define UNLOCK(l, r) \
    do { \
    if ((r = pthread_mutex_unlock(l)) != 0) { \
        PERRORSTR(r); \
    }\
    } while(0);

/**
 * @def     WAIT_DO()
 * @brief   Esegue pthread_cond_wait() sulla variabile di condizione c e la mutex l, salva il valore ritornato in r, 
 *          e in caso di errore stampa l'errore ed esegue l'istruzione y.
 *
 * @param c Riferimento alla variabile di condizione
 * @param l Riferimento alla mutex
 * @param r Dove salvare il valore ritornato da pthread_cond_wait()
 * @param y Istruzione da eseguire in caso di errore
 */
#define WAIT_DO(c, l, r, y) \
    do { \
    if ((r = pthread_cond_wait(c, l)) != 0) { \
        PERRORSTR(r); \
        { y; } \
    }\
    } while(0);

/**
 * @def     WAIT_DO()
 * @brief   Esegue pthread_cond_wait() sulla variabile di condizione c e la mutex l, salva il valore ritornato in r, 
 *          e in caso di errore stampa l'errore.
 *
 * @param c Riferimento alla variabile di condizione
 * @param l Riferimento alla mutex
 * @param r Dove salvare il valore ritornato da pthread_cond_wait()
 */
#define WAIT(c, l, r) \
    do { \
    if ((r = pthread_cond_wait(c, l)) != 0) { \
        PERRORSTR(r); \
    }\
    } while(0);

/**
 * @def     BCAST_DO()
 * @brief   Esegue pthread_cond_broadcast() sulla variabile di condizione c, salva il valore ritornato in r, e in caso di 
 *          errore stampa l'errore ed esegue l'istruzione y.
 *
 * @param c Riferimento alla variabile di condizione
 * @param r Dove salvare il valore ritornato da pthread_cond_broadcast()
 * @param y Istruzione da eseguire in caso di errore
 */
#define BCAST_DO(c, r, y) \
    do { \
    if ((r = pthread_cond_broadcast(c)) != 0) { \
        PERRORSTR(r); \
        { y; } \
    } \
    } while(0);

/**
 * @def     BCAST()
 * @brief   Esegue pthread_cond_broadcast() sulla variabile di condizione c, salva il valore ritornato in r e in caso 
 *          di errore stampa l'errore.
 *
 * @param c Riferimento alla variabile di condizione
 * @param r Dove salvare il valore ritornato da pthread_cond_broadcast()
 */
#define BCAST(c, r) \
    do { \
    if ((r = pthread_cond_broadcast(c)) != 0) { \
        PERRORSTR(r); \
    } \
    } while(0);

/**
 * @def     SIGNAL_DO()
 * @brief   Esegue pthread_cond_signal() sulla variabile di condizione c, salva il valore ritornato in r e in caso di 
 *          errore stampa l'errore ed esegue l'istruzione y.
 *
 * @param c Riferimento alla variabile di condizione
 * @param r Dove salvare il valore ritornato da pthread_cond_signal()
 * @param y Istruzione da eseguire in caso di errore
 */
#define SIGNAL_DO(c, r, y) \
    do { \
    if ((r = pthread_cond_signal(c)) != 0) { \
        PERRORSTR(r); \
        { y; } \
    } \
    } while(0);

/**
 * @def     SIGNAL()
 * @brief   Esegue pthread_cond_signal() sulla variabile di condizione c, salva il valore ritornato in r e in caso di 
 *          errore stampa l'errore.
 *
 * @param c Riferimento alla variabile di condizione
 * @param r Dove salvare il valore ritornato da pthread_cond_signal()
 */
#define SIGNAL(c, r) \
    do { \
    if ((r = pthread_cond_signal(c)) != 0) { \
        PERRORSTR(r); \
    } \
    } while(0);

/**
 * @def     NEQ0_DO()
 * @brief   Esegue X, salva il valore ritornato in r e nel caso in cui il valore ritornato sia diverso da 0, 
 *          stampa l'errore ed esegue l'istruzione y.
 *
 * @param X Istruzione da eseguire
 * @param r Dove salvare il valore ritornato da X
 * @param y Istruzione da eseguire in caso di errore
 */
#define NEQ0_DO(X, r, y) \
    do { \
    if ((r = (X)) != 0) { \
        PERRORSTR(r) \
        { y; } \
    } \
    } while(0);

/**
 * @def     NEQ0()
 * @brief   Esegue X, salva il valore ritornato in r e nel caso in cui il valore ritornato sia diverso da 0, stampa l'errore.
 *
 * @param X Istruzione da eseguire
 * @param r Dove salvare il valore ritornato da X
 */
#define NEQ0(X, r) \
    do { \
    if ((r = (X)) != 0) { \
        PERRORSTR(r) \
    } \
    } while(0);

/**
 * @def     EQM1_DO()
 * @brief   Esegue X, salva il valore ritornato in r e nel caso in cui il valore ritornato sia -1, stampa l'errore ed 
 *          esegue l'istruzione y.
 *
 * @param X Istruzione da eseguire
 * @param r Dove salvare il valore ritornato da X
 * @param y Istruzione da eseguire in caso di errore
 */
#define EQM1_DO(X, r, y) \
    do { \
    if ((r = (X)) == -1) { \
        PERRORSTR(errno) \
        { y; } \
    } \
    } while(0);

/**
 * @def     EQM1()
 * @brief   Esegue X, salva il valore ritornato in r e nel caso in cui il valore ritornato sia -1, stampa l'errore.
 *
 * @param X Istruzione da eseguire
 * @param r Dove salvare il valore ritornato da X
 */
#define EQM1(X, r) \
    do { \
    if ((r = (X)) == -1) { \
        PERRORSTR(errno) \
    } \
    } while(0);

/**
 * @def     EQNULL_DO()
 * @brief   Esegue X, salva il valore ritornato in r e nel caso in cui ritorni NULL, stampa l'errore ed esegue 
 *          l'istruzione y.
 *
 * @param X Istruzione da eseguire
 * @param r Dove salvare il valore ritornato da X
 * @param y Istruzione da eseguire in caso di errore
 */
#define EQNULL_DO(X, r, y) \
    do { \
    if (((r = (X)) == NULL)) { \
        PERRORSTR(errno) \
        { y; } \
    } \
    } while(0);

/**
 * @def     EQNULL()
 * @brief   Esegue X, salva il valore ritornato in r e nel caso in cui ritorni NULL, stampa l'errore.
 *
 * @param X Istruzione da eseguire
 * @param r Dove salvare il valore ritornato da X
 */
#define EQNULL(X, r) \
    do { \
    if (((r = (X)) == NULL)) { \
        PERRORSTR(errno) \
    } \
    } while(0);

/**
 * @def         CHECK()
 * @brief       Esegue X, salva il valore ritornato in r e nel caso in cui il confronto con l'operando op di tale valore 
 *              con val ha esito positivo, stampa l'errore.
 *
 * @param X     Istruzione da eseguire
 * @param val   Valore da confrontare
 * @param op    Operando di confronto
 * @param r     Dove salvare il valore ritornato da X
 */
#define CHECK(X, val, op, r) \
    do { \
        if ((r=(X)) op val) { \
            PERRORSTR(errno) \
        } \
    } while(0);

/**
 * @def         CHECK_NEQ()
 * @brief       Esegue X e nel caso in cui ritorni un valore diverso da val, stampa l'errore.
 *
 * @param X     Istruzione da eseguire
 * @param val   Valore da confrontare
 */
#define CHECK_NEQ(X, val) \
    do { \
        if ((X) != val) { \
            PERRORSTR(errno) \
        } \
    } while(0);

/**
 * @function    is_number()
 * @brief       Connverte s in un numero salvando il risultato in n.
 * 
 * @param s     La stringa che rappresenta un numero
 * @param n     Il numero prodotto dalla conversione della stringa
 * 
 * @return      1 in caso di fallimento e se s non è un numero, 
 *              2 in caso di fallimento e se è un numero troppo grande o troppo piccolo,
 *              0 in caso di successo.
 *              In caso di fallimento errno può assumere i seguenti valori:
 *              EINVAL se s è NULL o se la lunghezza di s è 0
 * @note        In caso di fallimento n non ha un valore significativo. 
 *              Può fallire e settare errno se si verificano gli errori specificati da strtol().
 */
int is_number(const char* s, long* n);

#endif /* UTIL_H */


