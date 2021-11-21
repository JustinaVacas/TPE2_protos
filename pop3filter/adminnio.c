#include "adminnio.h"

#define MAX_POOL 89

static unsigned pool_size = 0;
static struct admin *pool = NULL;

#define BUFFER_SIZE 64
#define ATTACHMENT(key) ((struct socks5 *)(key)->data)
#define N(x) (sizeof(x)/sizeof((x)[0]))

static const struct fd_handler admin_handler = {
    .handle_read    = admin_read,
    .handle_write   = admin_write,
    .handle_close   = admin_close,
    .handle_timeout = admin_timeout,
    .handle_block   = NULL //admin_block
};

//TODO admin_new()

static void
admin_destroy_(struct admin *a){
    free(a);
}

static void 
admin_destroy(admin* a) {
    if(a != NULL) {
        if(a->references == 1) {
            if(poolSize < maxPool) {
                a->next = pool;
                pool    = a;
                pool_size++;
            } else {
                admin_destroy_(a);
            }
        } else {
            a->references -= 1;
        }
    } 
}

void
admin_pool_destroy(void) {
    struct admin *next, *a;
    for(a = pool; a != NULL ; a = next) {
        next = a->next;
        admin_destroy(a);
    }
}

void
admin_passive_accept(struct selector_key *key){
    struct sockaddr_storage     client_addr;
    socklen_t                   client_addr_len = sizeof(client_addr);
    struct admin                *client_admin          = NULL;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr,
    
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    client_admin = admin_new();

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &admin_handler, OP_WRITE, client_admin)) {
        goto fail;
    }

    return ;

    fail:
    if(client != -1) {
        close(client);
    }
    admin_destroy(state);

}

static inline void
updateLastUsedTime(struct selector_key *key) {
    admin * a = ATTACHMENT(key);
    a->lastUse = time(NULL);
}

static void
admin_read(struct selector_key *key){
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    updateLastUsedTime(key);
    const enum adminState state = stm_handler_read(stm, key); //pide el estado al state_machine
    
    admin * adm = ATTACHMENT(key);
    adm->state = state; //setea el estado al admin
    if(ERROR == state || DONE == state) {
        admin_done(key);
    }
}

static void
admin_write(struct selector_key *key){
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    updateLastUsedTime(key);
    const enum adminState state = stm_handler_read(stm, key); //pide el estado al state_machine
    
    admin * adm = ATTACHMENT(key);
    adm->state = state; //setea el estado al admin
    if(ERROR == state || DONE == state) {
        admin_done(key);
    }
}

static void
admin_close(struct selector_key *key) {
    admin_destroy(ATTACHMENT(key));
}

static void
admin_done(struct selector_key* key) {
    const int fds[] = {
        ATTACHMENT(key)->client_fd
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
