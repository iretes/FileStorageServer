/**
 * @file    protocol.h
 * @brief   Header per la definizione del protocollo tra client e server.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

/* Path di default della socket */
#define DEFAULT_SOCKET_PATH "./storage_socket"

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

#endif /* PROTOCOL_H */