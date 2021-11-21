#ifndef TPE2_PROTOS_ADMINNIO_H
#define TPE2_PROTOS_ADMINNIO_H

#include <time.h>
#include "adminnio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include "buffer.h"
#include "logger.h"
#include "netutils.h"
#include "./buffer.h"
#include "./hello.h"
#include "./stm.h"
#include "./selector.h"

typedef enum adminState {
    HELLO,
    AUTHENTICATION,
    TRANSACTION,
    DONE,
    ERROR,
} adminState;

typedef struct admin {
    int client_fd;
    adminState state;
    bufferADT readBuffer;
    bufferADT writeBuffer;
    char clientAddress[MAX_STRING_IP_LENGTH];

    /** para el timeout */
    time_t lastUse;
    /** maquinas de estados */
    struct stateMachineCDT stm;
    /** Contador de referencias (cliente o origen que utiliza este estado)*/
    unsigned int references;
    /** siguiente en el pool */
    struct admin * next;
} admin;

void admin_passive_accept(struct selector_key *key);

#endif