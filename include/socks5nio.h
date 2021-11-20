#ifndef TPE2_PROTOS_SOCKS5NIO_H
#define TPE2_PROTOS_SOCKS5NIO_H

#include <sys/socket.h>
#include <stdint.h>

#include "./buffer.h"
#include "./hello.h"
#include "./stm.h"
#include "./selector.h"

#define BUFFER_SIZE 64
#define ATTACHMENT(key) ((struct socks5 *)(key)->data)
#define N(x) (sizeof(x)/sizeof((x)[0]))

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

/**
 * ---------------------------------------------
 * Máquina de estados general
 * ---------------------------------------------
 */
enum socks_v5state {
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
    HELLO_READ,

    /**
     * envía la respuesta del `hello' al cliente.
     *
     * Intereses:
     *     - OP_WRITE sobre client_fd
     *
     * Transiciones:
     *   - HELLO_WRITE  mientras queden bytes por enviar
     *   - REQUEST_READ cuando se enviaron todos los bytes
     *   - ERROR        ante cualquier error (IO/parseo)
     */
    HELLO_WRITE,


    // estados terminales
    DONE,
    ERROR,
};


/**
 * Si bien cada estado tiene su propio struct que le da un alcance
 * acotado, disponemos de la siguiente estructura para hacer una única
 * alocación cuando recibimos la conexión.
 *
 * Se utiliza un contador de referencias (references) para saber cuando debemos
 * liberarlo finalmente, y un pool para reusar alocaciones previas.
 */
struct socks5 {
    /** Información del cliente */
    int client_fd;
    struct sockaddr_storage client_addr;

    /** Información del origin */
    int origin_fd;
    struct addrinfo *origin_resolution;

    /** Maquinas de estados */
    struct state_machine stm;

    /** Estados para el client_fd */
    union {
        struct hello_st hello;
//        struct request_st request;
//        struct copy copy;
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

    /** Contador de referencias (cliente o origen que utiliza este estado)*/
    unsigned int references;
    /** Siguiente estructura */
    struct sock *next;
};

void socks5_destroy(struct socks5 *s);

void socksv5_pool_destroy(void);

void socksv5_passive_accept(struct selector_key *key);

void hello_read_init(const unsigned state, struct selector_key *key);

unsigned hello_read(struct selector_key *key);

unsigned hello_write(struct selector_key *key);

void socksv5_done(struct selector_key *key);

void socksv5_close(struct selector_key *key);

void socksv5_block(struct selector_key *key);

void socksv5_write(struct selector_key *key);

void socksv5_read(struct selector_key *key);


#endif 
