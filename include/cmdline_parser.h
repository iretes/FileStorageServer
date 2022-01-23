/**
 * @file                cmdline_parser.h
 * @brief               Intefaccia per il parsing degli argomenti della linea di comando del client.
 */

#ifndef CMDLINE_PARSER_H
#define CMDLINE_PARSER_H

#include <list.h>

/**
 * @function            cmdline_parser()
 * @brief               Effettua il parsing degli argomenti della linea di comando del client.
 * 
 * @param argc          Il numero di argomenti della linea di comando
 * @param argv          Gli argomenti della linea di comando
 * @param socket_path   Il riferimento al path della socket
 * 
 * @return              Una lista di cmdline_operation in caso di successo, 
 *                      NULL in caso di fallimento ed errno settato ad indicare l'errore.
 *                      In caso di fallimento errno può assumere i seguenti valori:
 *                      EINVAL se gli argomenti specificati non sono validi
 * @note                Può fallire e settare errno se si verificano gli errori specificati da 
 *                      list_create(), calloc(), cmdline_operation_create(), list_head_insert(), list_tail_insert(), 
 *                      list_head_remove(), list_reverse().
 */
list_t* cmdline_parser(int argc, char* argv[], char** socket_path);

#endif /* CMDLINE_PARSER_H */