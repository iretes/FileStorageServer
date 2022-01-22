/**
 * @file    protocol.h
 * @brief   Header per la definizione del protocollo tra client e server.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

/* Path di default della socket */
#define DEFAULT_SOCKET_PATH "./storage_socket"

/* Massima dimensione del path del socket file */
#define UNIX_PATH_MAX 108

/**
 * @enum	request_code_t
 * @brief	Codici di richiesta.
 */ 
typedef enum request_code {
	MIN_REQ_CODE 		= 0,
	OPEN_NO_FLAGS 		= 0,
	OPEN_CREATE 		= 1,
	OPEN_LOCK 			= 2,
	OPEN_CREATE_LOCK 	= 3,
	WRITE 				= 4,
	APPEND 				= 5,
	READ 				= 6,
	READN 				= 7,
	LOCK 				= 8,
	UNLOCK 				= 9,
	REMOVE 				= 10,
	CLOSE 				= 11,
	MAX_REQ_CODE 		= 11
} request_code_t;

/**
 * @enum	response_code_t
 * @brief	Codici di risposta.
 */ 
typedef enum response_code {
	MIN_RES_CODE 			= 0,
	OK 						= 0,
	NOT_RECOGNIZED_OP 		= 1,
	TOO_LONG_PATH 			= 2,
	TOO_LONG_CONTENT 		= 3,
	INVALID_PATH			= 4,
	FILE_NOT_EXISTS 		= 5,
	FILE_ALREADY_EXISTS 	= 6,
	FILE_ALREADY_OPEN 		= 7,
	FILE_ALREADY_LOCKED 	= 8,
	OPERATION_NOT_PERMITTED = 9,
	TEMPORARILY_UNAVAILABLE = 10,
	COULD_NOT_EVICT			= 11,
	MAX_RES_CODE 			= 11
} response_code_t;

/**
 * @function	req_code_to_str()
 * @brief		Restituisce una stringa che rappresenta il codice di richiesta.
 * 
 * @param code	Codice di richiesta
 * 
 * @return		Una stringa che rappresenta il codice di richiesta in caso di successo,
 * 		   		NULL in caso di fallimento, se il codice di richiesta non è un codice valido.
 */
char* req_code_to_str(request_code_t code);

/**
 * @function	resp_code_to_str()
 * @brief		Restituisce una stringa che rappresenta il codice di risposta.
 * 
 * @param code	Codice di risposta
 * 
 * @return		Una stringa che rappresenta il codice di risposta in caso di successo,
 * 		   		NULL in caso di fallimento, se il codice di risposta non è un codice valido.
 */
char* resp_code_to_str(response_code_t code);

#endif /* PROTOCOL_H */