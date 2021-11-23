#ifndef SERVERUTILS_H_
#define SERVERUTILS_H_

#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include "logger.h"
#include "util.h"
#include "args.h"

#define BUFSIZE 256
#define MAX_ADDR_BUFFER 128

// Create, bind, and listen a new TCP server socket
int create_socket(char * address, int port, int family, int protocol, int max_pending_connections, char * err_msg);

#endif 
