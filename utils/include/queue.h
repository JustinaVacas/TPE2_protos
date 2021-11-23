#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_ARGS_LENGHT 50

typedef enum command_type {
    CMD_OTHER     = -1,
    CMD_USER      =  0,
    CMD_PASS      =  1,
    CMD_QUIT      =  2,
    CMD_RETR      =  3,
    CMD_LIST      =  4,
    CMD_CAPA      =  5,
    CMD_TOP       =  6,
    CMD_UIDL      =  7,
    CMD_STAT      =  8,
} command_type;

typedef struct command_st{
    char * name;
    command_type type;
}command_st;

typedef struct node {
    command_type  command;
    int lenght;
    bool has_args;
    struct node * next;
}command_node;

typedef struct queue {
    uint16_t size;
    command_node * first;
    command_node * last;
}command_queue;

command_queue * new_command_queue();

command_node * dequeue(command_queue * queue);

command_node * peek(command_queue * queue);

void enqueue(command_queue * queue, command_node * node);

bool is_empty(command_queue * queue);

void free_node(command_node * node);

void destroy(command_queue * queue);

