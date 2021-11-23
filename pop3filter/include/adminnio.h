#ifndef TPE2_PROTOS_ADMINNIO_H
#define TPE2_PROTOS_ADMINNIO_H

#include <sys/socket.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "buffer.h"
#include "stm.h"
#include "selector.h"
#include "logger.h"
#include "args.h"
#include "pop3proxynio.h"

#define ADMIN_AUTH ((uint8_t *)"PASS")
#define ADMIN_AUTH_STR "PASS"

#define DGRAM_SIZE 512              // 2 bytes
#define DATA_SIZE (DGRAM_SIZE - 6) 
#define MIN_DGRAM 6                 // 5+1 AUTH + CMD: "PASS 1" 
#define COMMAND_SIZE 3              // STATS, GET_TIMEOUT, SET_TIMEOUT
#define SIZE 5
#define ASCII 48

extern size_t historic_connections;
extern size_t current_connections;
extern size_t transferred_bytes;
// extern stat_type * stat;
extern float  time_out;

typedef struct admin_request {
    uint8_t     auth[SIZE];         // 5 bytes
    uint8_t     command;            // 1 byte       
    uint8_t     data[DATA_SIZE];    // 2 bytes
} admin_request;                           

// typedef struct admin_response {
//     uint8_t     status;                 // 1 byte    
//     uint8_t     data[DGRAM_SIZE - 1];   // 2 bytes
// } admin_response;

enum admin_commands {
    STATS = 0,
    GET_TIMEOUT,
    SET_TIMEOUT
};

enum stats {
    OK = 0,
    UNSOPPORTED_COMMAND, // Error del comando
    INVALID_ARGS,        // Error de argumentos
    UNAUTHORIZED,         // No es AUTH
};

void admin_passive_accept(struct selector_key *key);
void sent(int fd, char * response,size_t n,struct sockaddr_storage client_addr, socklen_t client_addr_len);
void stats_handler(int fd, int request_len, struct admin_request * req, struct sockaddr_storage client_addr, size_t client_addr_len);
void get_timeout_handler(int fd, int request_len, struct admin_request * request, struct sockaddr_storage client_addr, size_t client_addr_len);
void set_timeout_handler(int fd, int request_len, struct admin_request * request, struct sockaddr_storage client_addr, size_t client_addr_len);

#endif
