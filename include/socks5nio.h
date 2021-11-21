#ifndef TPE2_PROTOS_SOCKS5NIO_H
#define TPE2_PROTOS_SOCKS5NIO_H

#include <sys/socket.h>
#include <stdint.h>

#include "buffer.h"
#include "hello.h"
#include "stm.h"
#include "selector.h"
#include "parser.h"
#include "logger.h"
#include "util.h"

#define BUFFER_SIZE 64
#define ATTACHMENT(key) ((struct pop3 *)(key)->data)
#define N(x) (sizeof(x)/sizeof((x)[0]))
#define COMMANDS 12

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct t_command {
    char * command;
} t_command;

typedef struct parser* ptr_parser;

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

struct metrics proxy_metrics;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Definición de variables para cada estado
 */

/** usado por HELLO_READ, HELLO_WRITE */
struct hello_st {
    /** buffer utilizado para I/O */
    buffer *rb, *wb;
    struct hello_parser parser;
    /** el método de autenticación seleccionado */
    uint8_t method;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * Estructura con lo necesario para parsear commands enviados por
 * un cliente.
 */
struct request_st{
    //queueADT            commands; 
    bool                waitingResponse;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Máquina de estados general
 */

enum pop3_state {

    RESOLVE_ORIGIN,
    CONNECT_ORIGIN,
    /**
     * recibe el mensaje `hello` del cliente, y lo procesa
     *
     * Intereses:
     *     - OP_READ sobre client_fd
     *
     * Transiciones:
     *   - HELLO_READ  mientras el mensaje no esté completo
     *   - HELLO_WRITE cuando está completo
     *   - ERROR       ante cualquier error (IO/parseo)
     */
    HELLO,
    CAPA,
    REQUEST,
    RESPONSE,
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

struct pop3 {
    /** Información del cliente */
    int client_fd;
    struct sockaddr_storage client_addr;

    /** Información del origin */
    int origin_fd;
    struct addrinfo *origin_resolution;
    struct addrinfo *current_origin_resolution;

    /** CAPA */
    bool pipelining;

    /** Maquinas de estados */
    struct state_machine stm;

    /** Estados para el client_fd */
    union {
        struct hello_st hello;
        struct request_st request;
    } client;

    /** Estados para el origin_fd */

//    union{
//        struct connecting conn;
//        strut copy copy;
//    } orig;

    /** Buffers */
    buffer read_buffer;
    uint8_t read_buffer_space[BUFFER_SIZE];
    buffer write_buffer;
    uint8_t write_buffer_space[BUFFER_SIZE];

    /** Parsers */
    ptr_parser parsers[COMMANDS];

    /** Contador de referencias (cliente o origen que utiliza este estado)*/
    unsigned int references;
    /** Siguiente estructura */
    struct pop3 *next;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void pop3_passive_accept(struct selector_key *key);

#endif 
