/**
 * @file    config.h
 * @brief   Interfaccia per il parsing del file di configurazione.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#include <eviction_policy.h>

/* Chiave riconosciuta nel file di configurazione per il numero di thread workers */
#define N_WORKERS_STR "n_workers"
/* Chiave riconosciuta nel file di configurazione per la dimensione massima della coda di task pendenti del pool */           
#define DIM_WORKERS_QUEUE_STR "dim_workers_queue"
/* Chiave riconosciuta nel file di configurazione per il massimo numero di file memorizzabili */
#define MAX_FILE_NUM_STR "max_file_num"
/* Chiave riconosciuta nel file di configurazione per il massimo numero di bytes memorizzabili*/
#define MAX_BYTES_STR "max_bytes"
/* Chiave riconosciuta nel file di configurazione per il massimo numero di lock da utilizzare per l'accesso ai files */
#define MAX_LOCKS_STR "max_locks"
/* Chiave riconosciuta nel file di configurazione per il numero atteso di client contemporaneamente connessi */
#define EXPECTED_CLIENTS_STR "expected_clients"
/* Chiave riconosciuta nel file di configurazione per il path della socket */
#define SOCKET_PATH_STR "socket_path_str"
/* Chiave riconosciuta nel file di configurazione per il path del file di log */
#define LOG_FILE_STR "log_file_path"
/* Chiave riconosciuta nel file di configurazione per la politica di espulsione */
#define EVICTION_POLICY_STR "eviction_policy"

/* Path di default del file di configurazione */
#define DEFAULT_CONFIG_PATH "./config.txt"

/* Valore di default del numero di thread workers */
#define DEFAULT_N_WORKERS 4
/* Valore di default della  dimensione massima della coda di task pendenti del pool */
#define DEFAULT_DIM_WORKERS_QUEUE SIZE_MAX
/* Valore di default del massimo numero di file memorizzabili */
#define DEFAULT_MAX_FILES 10
/* Valore di default del massimo numero di bytes memorizzabili */
#define DEFAULT_MAX_BYTES 1000000
/* Valore di default del massimo numero di lock da utilizzare per l'accesso ai files */
#define DEFAULT_MAX_LOCKS 100
/* Valore di default del numero atteso di client contemporaneamente connessi */
#define DEFAULT_EXPECTED_CLIENTS 10
/* Valore di default del path del file di log */
#define DEFAULT_LOG_PATH "./log.csv"
/* Valore di default della politica di espulsione */
#define DEFAULT_EVICTION_POLICY FIFO

/* Massima dimensione di una linea del file di configurazione */
#define CONFIG_LINE_SIZE 1024

/**
 * @struct                  config_t
 * @brief                   Parametri di configurazione.
 *
 * @var n_workers           Numero di thread workers
 * @var dim_workers_queue   Dimensione massima della coda di task pendenti del pool
 * @var max_file_num        Massimo numero di file memorizzabili
 * @var max_bytes           Massimo numero di bytes memorizzabili
 * @var max_locks           Massimo numero di lock da utilizzare per l'accesso ai files
 * @var expected_clients    Numero atteso di client contemporaneamente connessi
 * @var socket_path         Path della socket per la connessione con i clienti
 * @var log_file_path       Path del file di log
 * @var eviction_policy     Politica di espulsione
 */
typedef struct config {
    size_t n_workers;
    size_t dim_workers_queue;
    size_t max_file_num;
    size_t max_bytes;
    size_t max_locks;
    size_t expected_clients;
    char* socket_path;
    char* log_file_path;
    eviction_policy_t eviction_policy;
} config_t;

/**
 * @function        config_parse()
 * @brief           Effettua il parsing del file di configurazione.
 *
 * @param config    Puntatore alla struttura che raccoglie i parametri di configurazione
 * @param filepath  Path del file di configurazione
 *
 * @return          0 in caso di successo, -1 in caso di fallimento.
 */
int config_parser(config_t *config, char* filepath);

#endif /* CONFIG_H */