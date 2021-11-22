/**
 * proxyproxynio.c  - controla el flujo de un proxy SOCKSv5 (sockets no bloqueantes)
 */
#include <pop3proxynio.h>

struct nodeCDT {
    int command;
    uint8_t command_len;
    bool has_args;
    LIST_ENTRY(nodeCDT) pointers;
};

static const struct state_definition client_statbl[];

command_st commands[] = {{"CAPA"}, {"USER "}, {"PASS "}, {"QUIT"}, {"RETR "},{"LIST"},{"STAT"},{"DEL"},{"UIDL"},{"RSET"},{"NOOP"},{"TOP"}};

struct parser_definition * def[COMMANDS];
struct parser_definition * eol_def;
struct parser_definition * eoml_def;

void initialize_parsers(struct proxy * proxy) {
    // inicializamos los parsers de los comandos y los EOL
	for (int i = 0; i < COMMANDS; i++)
    {
        def[i] = malloc(sizeof(struct parser_definition));
        struct parser_definition aux = parser_utils_strcmpi(commands[i].name);
        memcpy(def[i], &aux, sizeof(struct parser_definition));
        proxy->parsers[i] = parser_init(parser_no_classes(), def[i]);
    }

    eol_def = malloc(sizeof(struct parser_definition));
    struct parser_definition aux2 = parser_utils_strcmpi("\r\n");
    memcpy(eol_def, &aux2, sizeof(struct parser_definition));
    proxy->eol = parser_init(parser_no_classes(), eol_def);

    eoml_def = malloc(sizeof(struct parser_definition));
    struct parser_definition aux3 = parser_utils_strcmpi("\r\n.\r\n");
    memcpy(eoml_def, &aux3, sizeof(struct parser_definition));
    proxy->eoml = parser_init(parser_no_classes(), eoml_def);
}

////////////////////////////////////////////////////////////////////////////////
// PROXY proxy - CREACIÓN Y DESTRUCCIÓN
////////////////////////////////////////////////////////////////////////////////

/* Pool de estructuras proxy */
static unsigned pool_size = 0;
static struct proxy *pool = NULL;

/* Crea un proxy proxy */
static struct proxy * proxy_new(int client_fd) {

    struct proxy * proxy;
    log(INFO, "Creating new proxy for client_id: %d", client_fd);

    if (pool == NULL) {
        proxy = malloc(sizeof(*proxy));
    }
    else {
        proxy = pool;
        pool = pool->next;
        proxy->next = 0;
    }

    memset(proxy, 0x00, sizeof(*proxy));
    proxy->client_fd = client_fd;
    proxy->origin_fd = -1;
    proxy->pipelining = false;
    proxy->session.last_use = time(NULL);

    proxy->stm.initial = RESOLVE;
    proxy->stm.max_state = FAILURE;
    proxy->stm.states = client_statbl;
    stm_init(&proxy->stm);

    buffer_init(&proxy->read_buffer, BUFFER_SIZE, proxy->read_buffer_space);
    buffer_init(&proxy->write_buffer, BUFFER_SIZE, proxy->write_buffer_space);
    
    initialize_parsers(proxy);
    
    proxy->references = 1;
    return proxy;
}

/* realmente destruye */
static void
proxy_destroy_(struct proxy* s) {
    if(s->origin_resolution != NULL) {
        freeaddrinfo(s->origin_resolution);
        s->origin_resolution = NULL;
    }
    free(s);
}

/**
 * destruye un  `struct proxy', tiene en cuenta las referencias
 * y el pool de objetos.
 */
static void
proxy_destroy(struct proxy *s) {
    if(s == NULL) {
        // nada para hacer
    } else if(s->references == 1) {
        if(s != NULL) {
            if(pool_size < MAX_POOL) {
                s->next = pool;
                pool    = s;
                pool_size++;
            } else {
                proxy_destroy_(s);
            }
        }
    } else {
        s->references -= 1;
    }
}

void
proxy_pool_destroy(void) {
    struct proxy *next, *s;
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
static void proxy_read   (struct selector_key *key);
static void proxy_write  (struct selector_key *key);
static void proxy_close  (struct selector_key *key);
static void proxy_block  (struct selector_key *key);
static void proxy_timeout  (struct selector_key *key);

static const struct fd_handler proxy_handler = {
    .handle_read   = proxy_read,
    .handle_write  = proxy_write,
    .handle_close  = proxy_close,
    .handle_block  = proxy_block,
    .handle_timeout = proxy_timeout,
};

/** 
    Handlers top level de la conexión pasiva.
    son los que emiten los eventos a la maquina de estados. 
*/
static void proxy_done(struct selector_key* key);
static inline void update_last_use(struct selector_key * key);

static void
proxy_read(struct selector_key *key) {
    update_last_use(key);
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum proxy_state st = stm_handler_read(stm, key);

    if(FAILURE == st || DONE == st) {
        proxy_done(key);
    }
}

static void
proxy_write(struct selector_key *key) {
    update_last_use(key);
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum proxy_state st = stm_handler_write(stm, key);

    if(FAILURE == st || DONE == st) {
        proxy_done(key);
    }
}

static void
proxy_block(struct selector_key *key) {
    update_last_use(key);
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum proxy_state st = stm_handler_block(stm, key);

    if(FAILURE == st || DONE == st) {
        proxy_done(key);
    }
}

static void
proxy_close(struct selector_key *key) {
    proxy_destroy(ATTACHMENT(key));
}

/**
 * Actualiza la ultima interacción de una sesión.
 */
static inline void update_last_use(struct selector_key * key) {
    struct proxy * proxy = ATTACHMENT(key);
    proxy->session.last_use = time(NULL);
}

/**
 * Timeout de una sesión.
 */
static void proxy_timeout(struct selector_key* key) {
    struct proxy *proxy = ATTACHMENT(key);
    if(proxy != NULL && difftime(time(NULL), proxy->session.last_use) >= TIMEOUT) {
        log(INFO, "%s\n", "Timeout");
        proxy->error_sender.message = "-ERR Disconnected for inactivity.\r\n";
        if(selector_set_interest(key->s, proxy->client_fd, OP_WRITE) != SELECTOR_SUCCESS)
            proxy_done(key);
        else 
            jump(&proxy->stm, SEND_ERROR_MSG, key);
    }
}

static void
proxy_done(struct selector_key* key) {
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
    struct proxy * proxy = NULL;
    int client = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);

    if(client == -1) {
        goto fail;
    }

    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }

    proxy = proxy_new(client);
    
    if(proxy == NULL) {
        goto fail;
    }

    memcpy(&proxy->client_addr, &client_addr, client_addr_len);

    if(selector_register(key->s, client, &proxy_handler, OP_WRITE, proxy) != SELECTOR_SUCCESS) {
        goto fail;
    }

    sockaddr_to_human(proxy->session.client_string, SOCKADDR_TO_HUMAN_MIN, (struct sockaddr*) &client_addr);
    log(INFO, "Accepting new client with address: %s.", proxy->session.client_string);

    proxy_metrics.activeConnections++;
    proxy_metrics.totalConnections++;

    return;
    
fail:
    if(client != -1) {
        close(client);
    }
    proxy_destroy(proxy);
}

////////////////////////////////////////////////////////////////////////////////
// RESOLVE
////////////////////////////////////////////////////////////////////////////////
static unsigned connect_ip(struct selector_key *key, int family, void * servaddr, socklen_t servaddr_size);
static unsigned connect_fqdn(struct selector_key* key);

/**
 * Realiza la resolución de DNS bloqueante.
 *
 * Una vez resuelto notifica al selector para que el evento esté
 * disponible en la próxima iteración.
 */
static void * resolve_blocking(void * data) {
    struct selector_key* key = (struct selector_key*)data;
    struct proxy * proxy = ATTACHMENT(key);

    pthread_detach(pthread_self());
    proxy->origin_resolution = 0;
    struct addrinfo hints = {
        .ai_family    = AF_UNSPEC,    
        /** Permite IPv4 o IPv6. */
        .ai_socktype  = SOCK_STREAM,  
        .ai_flags     = AI_PASSIVE,   
        .ai_protocol  = 0,        
        .ai_canonname = NULL,
        .ai_addr      = NULL,
        .ai_next      = NULL,
    };

    char buff[7];
    snprintf(buff, sizeof(buff), "%d", htons(args->origin_port));

    if (getaddrinfo(args->origin_address, buff, &hints, &proxy->origin_resolution) != 0) {
        log(ERROR, "%s %s:%d", "DNS resolution error for domain: ", args->origin_address, args->origin_port);
        goto finally;
    }

    proxy->current_origin_resolution = proxy->origin_resolution;
    char  buffer[SOCKADDR_TO_HUMAN_MIN];
    log(INFO, "Resolve blocking found current address: %s", sockaddr_to_human(buffer, SOCKADDR_TO_HUMAN_MIN, proxy->current_origin_resolution->ai_addr))

finally:
    selector_notify_block(key->s, key->fd);
    free(data);
    return 0;
}

static unsigned resolve(struct selector_key* key) {
    log(INFO, "Resolving origin %s:%d for client: %s", args->origin_address, args->origin_port, ATTACHMENT(key)->session.client_string);
    pthread_t thread;

    struct sockaddr_in servaddr;
    memset(&(servaddr), 0, sizeof(servaddr));

    struct sockaddr_in6 servaddr6;
    memset(&(servaddr6), 0, sizeof(servaddr6));

    if(inet_pton(AF_INET, args->origin_address, &(servaddr.sin_addr)) == 1) {
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(args->origin_port);
        return connect_ip(key, AF_INET, (void *) &servaddr, sizeof(servaddr));

    }else if(inet_pton(AF_INET6, args->origin_address, &(servaddr6.sin6_addr)) == 1){
        servaddr6.sin6_family = AF_INET6;
        servaddr6.sin6_port = htons(args->origin_port);
        return connect_ip(key, AF_INET6, (void *) &servaddr6, sizeof(servaddr6));
    }

    struct selector_key * k = malloc(sizeof(*key));
    if (k == NULL)
        return FAILURE;

    memcpy(k, key, sizeof(*k));
    if (pthread_create(&thread, 0, resolve_blocking, k) == -1 || selector_set_interest_key(key, OP_NOOP) != SELECTOR_SUCCESS){
        
        return SEND_ERROR_MSG;
    }
    return RESOLVE;
}

/**
 * Procesa el resultado de la resolución de nombres. 
 */
static unsigned resolve_done(struct selector_key* key) {
    struct proxy* proxy = ATTACHMENT(key);
    return proxy->origin_resolution == NULL ? FAILURE : connect_fqdn(key);
}

/**
 * Intenta conectarse cuando la dirección es IP. 
 */
static unsigned connect_ip(struct selector_key *key, int family, void * servaddr, socklen_t servaddr_size) {
    char  buffer[SOCKADDR_TO_HUMAN_MIN];
    log(INFO, "Connecting by IP to: %s, type %d", sockaddr_to_human(buffer, SOCKADDR_TO_HUMAN_MIN, servaddr), family);

    struct proxy* proxy = ATTACHMENT(key);

    proxy->origin_fd = socket(family, SOCK_STREAM, IPPROTO_TCP);

    if(proxy->origin_fd < 0)
        goto finally;
    if(selector_fd_set_nio(proxy->origin_fd) == -1)
        goto finally;

    log(DEBUG, "Socket created succesfully in fd %d", proxy->origin_fd);
    errno = 0;
    if(connect(proxy->origin_fd, (struct sockaddr *) servaddr, servaddr_size) == -1) {
        log(DEBUG, "Passed connect with errno %d", errno);
        if(errno == EINPROGRESS) {
            /**
             * Es esperable,  tenemos que esperar a la conexión.
             */
            if (selector_set_interest_key(key, OP_NOOP) != SELECTOR_SUCCESS) {
                goto finally;
            }
            log(DEBUG, "%s", "Selector key interest set succesfully");
            if (selector_register(key->s, proxy->origin_fd, &proxy_handler, OP_WRITE, key->data) != SELECTOR_SUCCESS) {
                goto finally;
            }
            log(DEBUG, "%s", "Selector resgitered succesfully");
            return CONNECT;
        }
    } 
    else {
        log(ERROR, "Problem: connected to origin server without wait. Client Address: %s", proxy->session.client_string);
        abort();
    }

finally:
    log(ERROR, "Client %s could not connect to origin %s:%d", proxy->session.client_string, args->origin_address, args->origin_port);
    if(proxy->origin_fd != -1){
        close(proxy->origin_fd);
        proxy->origin_fd = -1;
    }  
    return SEND_ERROR_MSG;
}

static unsigned connect_fqdn(struct selector_key* key) {
    log(INFO, "Connecting by address to: %s", args->origin_address);
    struct proxy* proxy = ATTACHMENT(key);
    
    int sock = -1;
    while (proxy->current_origin_resolution != NULL && sock == -1) {
        sock = socket(proxy->current_origin_resolution->ai_family, proxy->current_origin_resolution->ai_socktype, proxy->current_origin_resolution->ai_protocol);
        if (sock < 0) {
            close(sock);
            sock = -1;
            proxy->current_origin_resolution = proxy->current_origin_resolution->ai_next;
        }else {
            if (selector_fd_set_nio(sock) == -1){
                log(ERROR, "Error setting origin socket fd %d non blocking for Client: %s", sock, proxy->session.client_string);
                close(sock);
                sock = -1;
                proxy->current_origin_resolution = proxy->current_origin_resolution->ai_next; 
                continue;
            }
            if (connect(sock,  proxy->current_origin_resolution->ai_addr, proxy->current_origin_resolution->ai_addrlen) == -1 && errno == EINPROGRESS) {
                if (selector_set_interest_key(key, OP_NOOP) != SELECTOR_SUCCESS || selector_register(key->s, sock, &proxy_handler, OP_WRITE, key->data) != SELECTOR_SUCCESS) {
                    log(ERROR, "Error setting or registering selector in origin connection for Client: %s", proxy->session.client_string);
                    close(sock);
                    sock = -1;
                    proxy->current_origin_resolution = proxy->current_origin_resolution->ai_next; 
                    continue;
                }
            } 
            else {
                log(ERROR, "Problem: connected to origin server without wait. Client Address: %s", proxy->session.client_string);
                abort();
            }
        }
    }
    
    if(sock == -1) {
        log(ERROR, "Client %s could not connect to origin %s:%d",proxy->session.client_string, args->origin_address, args->origin_port);
        proxy->error_sender.message = "-ERR Connection refused.\r\n";
        if (selector_set_interest(key->s, proxy->client_fd, OP_WRITE) != SELECTOR_SUCCESS)
            return FAILURE;
        return SEND_ERROR_MSG;
    }

    return CONNECT;
}

////////////////////////////////////////////////////////////////////////////////
// CONNECTING
////////////////////////////////////////////////////////////////////////////////

static unsigned connection_ready(struct selector_key* key) { 
    log(INFO, "Resolve function... for client: %s", ATTACHMENT(key)->session.client_string);   
    struct proxy * proxy = ATTACHMENT(key);
    int error = 0;
    socklen_t len = sizeof(error);

    if(getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
        selector_set_interest_key(key, OP_NOOP);
        if(proxy->current_origin_resolution != NULL)
            proxy->current_origin_resolution = proxy->current_origin_resolution->ai_next;

        if (proxy->current_origin_resolution == NULL)
        {
            log(ERROR, "Problem connecting to origin server. Client Address: %s", proxy->session.client_string);
            proxy->error_sender.message = "-ERR Connection refused.\r\n";
            if(selector_set_interest(key->s, proxy->client_fd, OP_WRITE) != SELECTOR_SUCCESS)
                return FAILURE;

            return SEND_ERROR_MSG;
        }
        return connect_fqdn(key);

    }

    if(proxy->origin_resolution != NULL){
        freeaddrinfo(proxy->origin_resolution);
        proxy->origin_resolution = 0;
    }

    if(selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS) {
        return FAILURE;
    }

    strcpy(proxy->session.origin_string, args->origin_address);
    proxy_metrics.totalConnections++;
    log(INFO, "Connection established. Client Address: %s; Origin Address: %s:%d.", proxy->session.client_string, proxy->session.origin_string, args->origin_port);
    return HELLO;
}

////////////////////////////////////////////////////////////////////////////////
// HELLO
////////////////////////////////////////////////////////////////////////////////

/** lee todos los bytes del mensaje de tipo `hello' y inicia su proceso */
static unsigned hello_read(struct selector_key *key) {
    struct proxy * proxy = ATTACHMENT(key);
    size_t nbyte;
    uint8_t * write_ptr = buffer_write_ptr(&proxy->write_buffer, &nbyte);
    ssize_t n = recv(proxy->origin_fd, write_ptr, nbyte, 0);

    if (n <= 0) { 
        log(ERROR, "Error in HELLO. Client Address: %s", proxy->session.client_string);
        proxy->error_sender.message = "-ERR\r\n";
        if (selector_set_interest(key->s, proxy->client_fd, OP_WRITE) != SELECTOR_SUCCESS)
            return FAILURE;
        
        return SEND_ERROR_MSG;
    }

    for(int i = 0; i < n; i++) {
        const struct parser_event * state = parser_feed(proxy->eol, write_ptr[i]);
        if(state->type == STRING_CMP_EQ) {
            if(selector_set_interest(key->s, proxy->client_fd, OP_WRITE) != SELECTOR_SUCCESS || selector_set_interest_key(key, OP_NOOP) != SELECTOR_SUCCESS)
                return FAILURE;
        } else if(state->type == STRING_CMP_NEQ) {
            parser_reset(proxy->eol);
        }
    }

    parser_reset(proxy->eol);
    buffer_write_adv(&proxy->write_buffer, n);
    proxy_metrics.bytesWriteBuffer += n;
    proxy_metrics.writesQtyWriteBuffer++;
    return HELLO;
}

static unsigned hello_write(struct selector_key* key) {
    struct proxy * proxy = ATTACHMENT(key);

    size_t nbyte;
    uint8_t * read_ptr = buffer_read_ptr(&proxy->write_buffer, &nbyte);
    ssize_t n = send(key->fd, read_ptr, nbyte, MSG_NOSIGNAL);

    if(n <= 0) {
        log(ERROR, "Initial hello has an error. Client Address: %s", proxy->session.client_string);
        proxy->error_sender.message = "-ERR\r\n";
        if (selector_set_interest(key->s, proxy->client_fd, OP_WRITE) != SELECTOR_SUCCESS)
            return FAILURE;
        
        return SEND_ERROR_MSG;
    }

    buffer_read_adv(&proxy->write_buffer, n);
    proxy_metrics.totalBytesToClient += n;

    if(!buffer_can_read(&proxy->write_buffer)) {
        if(selector_set_interest_key(key, OP_READ) != SELECTOR_SUCCESS || selector_set_interest(key->s, proxy->origin_fd, OP_NOOP) != SELECTOR_SUCCESS){
            
            return FAILURE;
        }
        return COPY;
    }
    return HELLO;
}

////////////////////////////////////////////////////////////////////////////////
// SEND_ERROR_MSG
////////////////////////////////////////////////////////////////////////////////

static unsigned write_error_msg(struct selector_key *key) {
    struct proxy * proxy = ATTACHMENT(key);
    unsigned ret = SEND_ERROR_MSG;

    if(proxy->error_sender.message == NULL)
        return FAILURE;
    if(proxy->error_sender.message_length == 0)
        proxy->error_sender.message_length = strlen(proxy->error_sender.message);
        
    log(DEBUG, "Enviando error: %s", proxy->error_sender.message);

    char *   ptr  = proxy->error_sender.message + proxy->error_sender.sended_size;
    ssize_t  size = proxy->error_sender.message_length - proxy->error_sender.sended_size;
    ssize_t  n    = send(proxy->client_fd, ptr, size, MSG_NOSIGNAL);
    if(n == -1) {
        shutdown(proxy->client_fd, SHUT_WR);
        ret = FAILURE;
    } else {
        proxy->error_sender.sended_size += n;
        if(proxy->error_sender.sended_size == proxy->error_sender.message_length) 
            ret = FAILURE;
    }
    return ret;
}


/** definicón de handlers para cada estado */
static const struct state_definition client_statbl[] = {
    //conectarse al proxy
    {    
        .state              = RESOLVE,
        .on_write_ready     = resolve,
        .on_block_ready     = resolve_done,
    },
    {    
        .state            = CONNECT,
        .on_write_ready   = connection_ready,
    },
    {    
        .state            = HELLO,
        .on_read_ready    = hello_read,
        .on_write_ready   = hello_write,
        //.on_departure     = hello_read_close,
    },
    {    
        .state            = CAPA,
    },
    {    
        .state            = COPY,
    },
    {
        .state            = SEND_ERROR_MSG,
        .on_write_ready    = write_error_msg,
    },
    {
        .state            = DONE,
    },
    {
        .state            = FAILURE,
    },
};


//struct parser_definition *defs[COMMANDS]; ESTA ARRIBA DEFINIDO

// // inicializamos los parsers de los comandos
// void init_parsers_defs(){
// 	for(int i=0; i < COMMANDS; i++){
// 		defs[i] = malloc(sizeof(struct parser_definition));
//         struct parser_definition parser = parser_utils_strcmpi(commands[i].name);
//         memcpy(defs[i], &parser, sizeof(struct parser_definition));
// 	}
//     //H si va
// 	struct parser_definition eol_def = parser_utils_strcmpi("\r\n");
// 	parser * eol_parser = parser_init(parser_no_classes(), &eol_def);
// }

// void init_parsers(struct proxy* proxy){
//     for (int i = 0; i < COMMANDS; i++) {
//         proxy->parsers[i] = parser_init(parser_no_classes(), defs[i]);
//     }
// }

// void reset_parsers(struct proxy* proxy){
// 	for (int i = 0; i < COMMANDS; i++){
// 		parser_reset(proxy->parsers[i]);
// 	}
// }
