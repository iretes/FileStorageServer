/**
* @file     protocol.c
* @brief    Implementazione di funzioni relative al protocollo tra client e server.
*/

#include <stdlib.h>

#include <protocol.h>

char* req_code_to_str(request_code_t code) {
	switch (code) {
		case OPEN_NO_FLAGS:
			return "OPEN_NO_FLAGS";
		case OPEN_CREATE:
			return "OPEN_CREATE";
		case OPEN_LOCK:
			return "OPEN_LOCK";
		case OPEN_CREATE_LOCK:
			return "OPEN_CREATE_LOCK";
		case WRITE:
			return "WRITE";
		case APPEND:
			return "APPEND";
		case READ:
			return "READ";
		case READN:
			return "READN";
		case LOCK:
			return "LOCK";
		case UNLOCK:
			return "UNLOCK";
		case REMOVE:
			return "REMOVE";
		case CLOSE:
			return "CLOSE";
		default: 
			return NULL;
	}
}

char* resp_code_to_str(response_code_t code) {
	switch (code) {
		case OK:
			return "OK";
		case NOT_RECOGNIZED_OP:
			return "NOT_RECOGNIZED_OP";
		case TOO_LONG_PATH:
			return "TOO_LONG_PATH";
		case TOO_LONG_CONTENT:
			return "TOO_LONG_CONTENT";
		case INVALID_PATH:
			return "INVALID_PATH";
		case FILE_NOT_EXISTS:
			return "FILE_NOT_EXISTS";
		case FILE_ALREADY_EXISTS:
			return "FILE_ALREADY_EXISTS";
		case FILE_ALREADY_OPEN:
			return "FILE_ALREADY_OPEN";
		case FILE_ALREADY_LOCKED:
			return "FILE_ALREADY_LOCKED";
		case OPERATION_NOT_PERMITTED:
			return "OPERATION_NOT_PERMITTED";
		case TEMPORARILY_UNAVAILABLE:
			return "TEMPORARILY_UNAVAILABLE";
		case COULD_NOT_EVICT:
			return "COULD_NOT_EVICT";
		default: 
			return NULL;
	}
}