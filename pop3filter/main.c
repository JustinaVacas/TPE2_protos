/**
 * main.c - servidor proxy socks concurrente
 *
 * Interpreta los argumentos de linea de comandos, y monta un socket
 * pasivo.
 *
 * Todas las conexiones entrantes se manejarán en este hilo.
 *
 * Se descargará en otro hilos las operaciones bloqueantes (resolución de
 * DNS utilizando getaddrinfo), pero toda esa complejidad está oculta en
 * el selector.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "serverutils.h"
#include "selector.h"
#include "socks5nio.h"

#define MAX_PENDING_CONNECTIONS 20
#define SELECTOR_INITIAL_ELEMENTS 1024
#define TIMEOUT 120.0

static bool done = false;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

int
main(const int argc, char *argv[]) {
    //Parseamos los argumentos
    struct pop3args * args = malloc(sizeof(struct pop3args));
    parse_args(argc, argv, args);

    // no tenemos nada que leer de stdin
    close(STDIN_FILENO);

    char * err_msg            = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    // Sockets fd
    int server_ipv4 = -1;
    int server_ipv6 = -1;
    int management_ipv4 = -1;
    int management_ipv6 = -1;

    // Inicializamos el servidor IPv4
    server_ipv4 = setup_server_socket(args->pop3_listen_address, AF_INET ,args->pop3_port, IPPROTO_TCP , MAX_PENDING_CONNECTIONS, err_msg);
	if(server_ipv4 < 0){
        if (err_msg == NULL)
        {
            err_msg = "Unable to create IPv4 socket.";
        }
        goto finally;
	}

    fprintf(stdout, "Listening IPv4 on port %d\n", args->pop3_port);

    // Inicializamos el servidor IPv6
    server_ipv6 = setup_server_socket(args->pop3_listen_address, AF_INET6 ,args->pop3_port, IPPROTO_TCP, MAX_PENDING_CONNECTIONS, err_msg);
	if(server_ipv6 < 0){
        if (err_msg == NULL)
        {
            err_msg = "Unable to create IPv6 socket.";
        }
        goto finally;
	}

    fprintf(stdout, "Listening IPv6 on port %d\n", args->pop3_port);

    // Inicializamos el servidor admin IPv4
    management_ipv4 = setup_server_socket(args->management_listen_address, AF_INET ,args->management_port, IPPROTO_UDP, MAX_PENDING_CONNECTIONS, err_msg);
	if(server_ipv6 < 0){
        if (err_msg == NULL)
        {
            err_msg = "Unable to create management IPv4 socket.";
        }
        goto finally;
	}

    fprintf(stdout, "Listening management IPv4 on port %d\n", args->management_port);

    // Inicializamos el servidor admin IPv6
    management_ipv6 = setup_server_socket(args->management_listen_address, AF_INET6 ,args->management_port, IPPROTO_UDP, MAX_PENDING_CONNECTIONS, err_msg);
	if(management_ipv6 < 0){
        if (err_msg == NULL)
        {
            err_msg = "Unable to create management IPv6 socket.";
        }
        goto finally;
	}

    fprintf(stdout, "Listening management IPv6 on port %d\n", args->management_port);

    // registrar sigterm es til para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    if(selector_fd_set_nio(server_ipv4) == -1 || selector_fd_set_nio(server_ipv6) == -1 || selector_fd_set_nio(management_ipv4) == -1 || selector_fd_set_nio(management_ipv6) == -1) {
        err_msg = "Error getting server socket flags";
        goto finally;
    }
    const struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec  = 20,
            .tv_nsec = 0,
        },
    };

    if(selector_init(&conf) != SELECTOR_SUCCESS) {
        err_msg = "Error initializing selector";
        goto finally;
    }

    selector = selector_new(SELECTOR_INITIAL_ELEMENTS);
    if(selector == NULL) {
        err_msg = "Unable to create selector";
        goto finally;
    }

    const struct fd_handler pop3_handler = {
        .handle_read       = pop3_passive_accept,
        .handle_write      = NULL,
        .handle_close      = NULL, // nada que liberar
        .handle_timeout    = NULL,
    };
    const struct fd_handler management_handler = {
        .handle_read       = NULL, //TODO: aca va la funcion de aceptar del admin
        .handle_write      = NULL,
        .handle_close      = NULL, // nada que liberar
        .handle_timeout    = NULL,
    };

    ss = selector_register(selector, server_ipv4, &pop3_handler, OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "Failed to register server IPv4 fd";
        goto finally;
    }

    ss = selector_register(selector, server_ipv6, &pop3_handler, OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "Failed to register server IPv6 fd";
        goto finally;
    }

    ss = selector_register(selector, management_ipv4, &management_handler, OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "Failed to register management IPv4 fd";
        goto finally;
    }

    ss = selector_register(selector, management_ipv6, &management_handler, OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "Failed to register management IPv6 fd";
        goto finally;
    }

    time_t lastTimeout = time(NULL);
    for(;!done;) {
        err_msg = NULL;
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "Failed serving";
            goto finally;
        }
        time_t current = time(NULL);
        if(difftime(current, lastTimeout) >= TIMEOUT/4) {
            lastTimeout = current;
            selector_timeout(selector);
        }
    }
    if(err_msg == NULL) {
        err_msg = "Closing...";
    }

    int ret = 0;

//Seccion de error 
finally:
    if(ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                                  ss == SELECTOR_IO
                                      ? strerror(errno)
                                      : selector_error(ss));
        ret = 2;
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if(selector != NULL) {
        selector_destroy(selector);
    }
    selector_close();

    if(server_ipv4 >= 0) {
        close(server_ipv4);
    }
    if(server_ipv6 >= 0) {
        close(server_ipv6);
    }
    if(management_ipv4 >= 0) {
        close(management_ipv4);
    }
    if(management_ipv6 >= 0) {
        close(management_ipv6);
    }
    return ret;
}
