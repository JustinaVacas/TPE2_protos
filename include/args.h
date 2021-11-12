#ifndef ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>

#define MAX_USERS 10

struct users {
    char *name;
    char *pass;
};

struct doh {
    char           *host;
    char           *ip;
    unsigned short  port;
    char           *path;
    char           *query;
};

struct pop3args {
    char * error_file;
    char * pop3_listen_address;
    char * management_listen_address;
    uint16_t management_port;
    uint16_t pop3_port;
    uint16_t origin_port;
    char * filter;
};

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void 
parse_args(const int argc, char **argv, struct pop3args *args);

#endif