#ifndef POP3CTL_H
#define POP3CTL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <client_args.h>
#include <errno.h> 

#include "logger.h"

#define TRUE 1
#define DGRAM_SIZE 512  
#define CMD_LEN 13
#define CMD_SIZE 3 
#define BUFFER 512

typedef struct admin_response{
    uint8_t     status;                 // 1 byte    
    uint8_t     data[DGRAM_SIZE - 1];   // 2 bytes
} admin_response;

int udpSocket(int port);
int main(int argc, char* argv[]);

#endif
