/**
 * socks5nio.c  - controla el flujo de un proxy SOCKSv5 (sockets no bloqueantes)
 */
#include<stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <time.h>
#include <unistd.h>  // close
#include <pthread.h>

#include <arpa/inet.h>

#include "hello.h"
#include "request.h"
#include "buffer.h"

#include "stm.h"
#include "socks5nio.h"
#include "netutils.h"

#define MAX_POOL 89

static const struct state_definition client_statbl[];

struct parser_definition *defs[COMMANDS];

t_command commands[] = {{"CAPA"}, {"USER"}, {"PASS"}, {"QUIT"}, {"RETR"},{"LIST"},{"STAT"},{"DEL"},{"UIDL"},{"RSET"},{"NOOP"},{"TOP"}}


////////////////////////////////////////////////////////////////////////////////
// PROXY POP3 - CREACIÓN Y DESTRUCCIÓN
////////////////////////////////////////////////////////////////////////////////

/* Pool de estructuras Pop3 */
//static const unsigned  pool_max = 50;
static unsigned pool_size = 0;
static struct pop3 *pool = NULL;

/* Crea un proxy Pop3 */
static struct pop3 * pop3_new(int client_fd) {
    struct pop3 * pop3;
    if (pool == NULL) {
        pop3 = malloc(sizeof(*pop3));
    }
    else {
        pop3 = pool;
        pool = pool->next;
        pop3->next = 0;
    }

    memset(pop3, 0x00, sizeof(*pop3));
    pop3->client_fd = client_fd;
    pop3->origin_fd = -1;
    pop3->pipelining = false;
    pop3->stm.initial = RESOLVE_ORIGIN;
    pop3->stm.max_state = FAILURE;
    pop3->stm.states = client_statbl;

    stm_init(&pop3->stm);

    buffer_init(&pop3->read_buffer, BUFFER_SIZE, pop3->read_buffer_space);
    buffer_init(&pop3->write_buffer, BUFFER_SIZE, pop3->write_buffer_space);
    //init_parsers(pop3);
    
    pop3->references = 1;
    return pop3;
}

/* realmente destruye */
static void
pop3_destroy_(struct pop3* s) {
    if(s->origin_resolution != NULL) {
        freeaddrinfo(s->origin_resolution);
        s->origin_resolution = 0;
    }
    free(s);
}

/**
 * destruye un  `struct pop3', tiene en cuenta las referencias
 * y el pool de objetos.
 */
static void
pop3_destroy(struct pop3 *s) {
    if(s == NULL) {
        // nada para hacer
    } else if(s->references == 1) {
        if(s != NULL) {
            if(pool_size < MAX_POOL) {
                s->next = pool;
                pool    = s;
                pool_size++;
            } else {
                pop3_destroy_(s);
            }
        }
    } else {
        s->references -= 1;
    }
}

void
pop3_pool_destroy(void) {
    struct pop3 *next, *s;
    for(s = pool; s != NULL ; s = next) {
        next = s->next;
        free(s);
    }
}

////////////////////////////////////////////////////////////////////////////////
// HANDLERS
////////////////////////////////////////////////////////////////////////////////

/**     Declaración forward de los handlers de selección de una conexión
 *      establecida entre un cliente y el proxy.
 */
static void pop3_read   (struct selector_key *key);
static void pop3_write  (struct selector_key *key);
static void pop3_block  (struct selector_key *key);
static void pop3_close  (struct selector_key *key);

static const struct fd_handler pop3_handler = {
    .handle_read   = pop3_read,
    .handle_write  = pop3_write,
    .handle_close  = pop3_close,
    .handle_block  = pop3_block,
};

/** 
    Handlers top level de la conexión pasiva.
    son los que emiten los eventos a la maquina de estados. 
*/
static void pop3_done(struct selector_key* key);

static void
pop3_read(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_state st = stm_handler_read(stm, key);

    if(FAILURE == st || DONE == st) {
        pop3_done(key);
    }
}

static void
pop3_write(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_state st = stm_handler_write(stm, key);

    if(FAILURE == st || DONE == st) {
        pop3_done(key);
    }
}

static void
pop3_block(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3_state st = stm_handler_block(stm, key);

    if(FAILURE == st || DONE == st) {
        pop3_done(key);
    }
}

static void
pop3_close(struct selector_key *key) {
    pop3_destroy(ATTACHMENT(key));
}

static void
pop3_done(struct selector_key* key) {
    const int fds[] = {
        ATTACHMENT(key)->client_fd,
        ATTACHMENT(key)->origin_fd,
    };
    for(unsigned i = 0; i < N(fds); i++) {
        if(fds[i] != -1) {
            if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {
                abort();
            }
            close(fds[i]);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// ACCEPT DE SOCKETS PASIVOS
////////////////////////////////////////////////////////////////////////////////

/** Intenta aceptar la nueva conexión entrante*/
void
pop3_passive_accept(struct selector_key *key) {
    struct sockaddr_storage  client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    struct pop3 * state = NULL;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);

    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }

    state = pop3_new(client);

    if(state == NULL) {
        goto fail;
    }

    memcpy(&state->client_addr, &client_addr, client_addr_len);

    if(selector_register(key->s, client, &pop3_handler, OP_WRITE, state) != SELECTOR_SUCCESS) {
        goto fail;
    }

    log(INFO, "Accepting new client.",NULL);

    proxy_metrics.activeConnections++;
    proxy_metrics.totalConnections++;

    return ;
    
    fail:
    if(client != -1) {
        close(client);
    }
    pop3_destroy(state);
}

////////////////////////////////////////////////////////////////////////////////
// HELLO
////////////////////////////////////////////////////////////////////////////////

/** callback del parser utilizado en `read_hello' */
static void
on_hello_method(struct hello_parser *p, const uint8_t method) {
    uint8_t *selected  = p->data;

    if(SOCKS_HELLO_NOAUTHENTICATION_REQUIRED == method) {
       *selected = method;
    }
}

/** inicializa las variables de los estados HELLO_â€¦ */
static void
hello_read_init(const unsigned state, struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->client.hello;

    d->rb                              = &(ATTACHMENT(key)->read_buffer);
    d->wb                              = &(ATTACHMENT(key)->write_buffer);
    d->parser.data                     = &d->method;
    d->parser.on_authentication_method = on_hello_method, hello_parser_init(&d->parser);
}

static unsigned
hello_process(const struct hello_st* d);

/** lee todos los bytes del mensaje de tipo `hello' y inicia su proceso */
static unsigned
hello_read(struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    unsigned  ret      = HELLO;
        bool  error    = false;
     uint8_t *ptr;
      size_t  count;
     ssize_t  n;

    ptr = buffer_write_ptr(d->rb, &count);
    n = recv(key->fd, ptr, count, 0);
    if(n > 0) {
        buffer_write_adv(d->rb, n);
        const enum hello_state st = hello_consume(d->rb, &d->parser, &error);
        if(hello_is_done(st, 0)) {
            if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
                ret = hello_process(d);
            } else {
                ret = FAILURE;
            }
        }
    } else {
        ret = FAILURE;
    }

    return error ? FAILURE : ret;
}

/** procesamiento del mensaje `hello' */
static unsigned
hello_process(const struct hello_st* d) {
    unsigned ret = HELLO;

    uint8_t m = d->method;
    const uint8_t r = (m == SOCKS_HELLO_NO_ACCEPTABLE_METHODS) ? 0xFF : 0x00;
    if (-1 == hello_marshall(d->wb, r)) {
        ret  = FAILURE;
    }
    if (SOCKS_HELLO_NO_ACCEPTABLE_METHODS == m) {
        ret  = FAILURE;
    }
    return ret;
}

/** definicón de handlers para cada estado */
static const struct state_definition client_statbl[] = {
    {
        .state            = HELLO,
        .on_arrival       = hello_read_init,
        //.on_departure     = hello_read, //antes decia hello_read_close
        .on_read_ready    = hello_read,
    },
};


//struct parser_definition *defs[COMMANDS]; ESTA ARRIBA DEFINIDO

// inicializamos los parsers de los comandos
void init_parsers_defs(){
	for(int i=0; i < COMMANDS; i++){
		defs[i] = malloc(sizeof(struct parser_definition));
        struct parser_definition parser = parser_utils_strcmpi(commands[i].name);
        memcpy(defs[i], &parser, sizeof(struct parser_definition));
	}
    //H si va
	struct parser_definition eol_def = parser_utils_strcmpi("\r\n");
	parser * eol_parser = parser_init(parser_no_classes(), &eol_def);
}

void init_parsers(struct pop3* pop3_ptr){
    for (int i = 0; i < COMMANDS; i++) {
        pop3_ptr->parsers[i] = parser_init(parser_no_classes(), defs[i]);
    }
}

void reset_parsers(struct pop3* pop3_ptr){
	for (int i = 0; i < COMMANDS; i++){
		parser_reset(pop3_ptr->parsers[i]);
	}
}