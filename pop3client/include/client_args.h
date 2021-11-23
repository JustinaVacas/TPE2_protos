#ifndef POP3CTL_ARGS_H
#define POP3CTL_ARGS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>

#define MAX_LINE 100

struct client_args {
    char *      admin_server_address;
    char *      admin_auth;             
    uint16_t    admin_server_port;
};

void parse_args(const int argc, char **argv,struct client_args *client_args);

#endif
