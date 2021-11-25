#ifndef TPE2_PROTOS_POP3PROXYNIO_H
#define TPE2_PROTOS_POP3PROXYNIO_H

#include <sys/socket.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <time.h>
#include <unistd.h>  // close
#include <pthread.h>
#include <sys/queue.h>

#include <arpa/inet.h>

#include "request.h"
#include "buffer.h"
#include "args.h"
#include "netutils.h"
#include "stm.h"
#include "selector.h"
#include "parser.h"
#include "logger.h"
#include "util.h"
#include "parser_utils.h"
#include "queue.h"
#include "adminnio.h"

#define BUFFER_SIZE 1024
#define ATTACHMENT(key) ((struct proxy *)(key)->data)
#define N(x) (sizeof(x)/sizeof((x)[0]))
#define COMMANDS 9
#define MAX_ARGS_LENGTH 40
#define MAX_POOL 89
#define TIMEOUT 120.0

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Estructura con lo necesario para enviar errores al cliente.
 */
typedef struct error_container {
    char * message;
    size_t message_length;
    size_t sended_size;
} error_container;

/**
 * Estructura de una sesión, guarda el nombre de usuario logeado en la sesion
 * un bool para saber si hay un usuario logeado las representaciones en string
 * de las direcciones utilizadas y la úĺtima vez que interaccionó la sesión.
 */
struct session{
    char                name[MAX_ARGS_LENGTH + 1];
    bool                is_auth;
    char                origin_string[SOCKADDR_TO_HUMAN_MIN];
    char                client_string[SOCKADDR_TO_HUMAN_MIN];
    time_t              last_use;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct stat_type {
    size_t historic_connections;
    size_t current_connections;
    size_t transferred_bytes;
} stat_type;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

struct metrics {
    /** Para saber los bytes copiados hacia el cliente, origin y filter */
    unsigned long long   totalBytesToOrigin;
    unsigned long long   totalBytesToClient;
    unsigned long long   totalBytesToFilter;

    /** Para el total de bytes escritos en los buffers */
    unsigned long long   bytesReadBuffer;
    unsigned long long   bytesWriteBuffer;
    unsigned long long   bytesFilterBuffer;

    /** Para poder sacar el average de uso de los buffers */
    unsigned long long   writesQtyReadBuffer;    
    unsigned long long   writesQtyWriteBuffer;
    unsigned long long   writesQtyFilterBuffer;

    unsigned long long   readsQtyReadBuffer;    
    unsigned long long   readsQtyWriteBuffer;
    unsigned long long   readsQtyFilterBuffer;

    unsigned long long   totalConnections;
    unsigned long long   activeConnections;

    unsigned long long   commandsFilteredQty;
};

extern struct metrics proxy_metrics;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Estructura con lo necesario para parsear commands enviados por
 * un cliente.
 */
struct request_st{
    command_queue * commands;
    int command_found;
    int command_lenght;
    bool command_has_args;
    int command_state[COMMANDS];
};

/**
 * Estructura con lo necesario para parsear commands enviados por
 * un cliente.
 */
struct response_st {
    //  comando a resolver
    command_type    command;
    bool            has_args;

    //  fds por si hay filtro
    int             write_fd;
    int             read_fd;

    //  cambia si es multiline
    struct parser * eol_parser;

    //  se usa por si hay que leer en varias pasadas
    unsigned        return_state;

    //  para poner PIPELINING en comando CAPA
    char*           add_pipelining;
    size_t          add_lenght;

    //  states for reading
    bool            read_init;
    bool            read_complete;
};

typedef struct capa_st {
    struct parser *     eol_parser;
    struct parser *     capa_parser;
    bool                pipelining;
    bool                checked;
    bool                started;
}capa_st;

/**
 * Máquina de estados general
 */

enum proxy_state {

    RESOLVE,
    CONNECT,
    HELLO,
    REQUEST,
    RESPONSE,
    CAPA,
    FILTER,
    SEND_ERROR_MSG,
    // estados terminales
    DONE,
    FAILURE
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * Si bien cada estado tiene su propio struct que le da un alcance
 * acotado, disponemos de la siguiente estructura para hacer una única
 * alocación cuando recibimos la conexión.
 *
 * Se utiliza un contador de referencias (references) para saber cuando debemos
 * liberarlo finalmente, y un pool para reusar alocaciones previas.
 */

struct proxy {
    /** Estructura de la sesión activa */
    struct session session;

    /** Información del cliente */
    int client_fd;
    struct sockaddr_storage client_addr;

    /** Información del origin */
    int origin_fd;
    struct addrinfo *origin_resolution;
    struct addrinfo *current_origin_resolution;

    //LIST_HEAD(command_list, command_node) commands;

    /** Error */
    error_container  error_sender;

    /** Maquinas de estados */
    struct state_machine stm;

    /** Parsers */
    struct parser * parsers[COMMANDS];
    struct parser * eol;
    struct parser * eoml;

    /** States */
    struct request_st   request;
    struct response_st  response;
    struct capa_st      capa;

    /** Buffers */
    buffer read_buffer; // El que lee del cliente y manda al origin             cliente --->  origen
    uint8_t read_buffer_space[BUFFER_SIZE];
    buffer write_buffer; // El que recibe del origin y escribe en el cliente    origen  ---> cliente    
    uint8_t write_buffer_space[BUFFER_SIZE];

    /** Contador de referencias (cliente o origen que utiliza este estado)*/
    unsigned int references;
    /** Siguiente estructura */
    struct proxy *next;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void pop3_passive_accept(struct selector_key *key);
void proxy_pool_destroy();
void initialize_parser_definitions();
void destroy_parser_definitions();

#endif 
