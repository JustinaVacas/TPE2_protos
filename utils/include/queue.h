#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

struct nodeCDT {
    int command;
    uint8_t command_len;
    bool has_args;
    struct nodeCDT * next;
};

struct queueCDT {
    uint16_t size;
    struct nodeCDT * first;
    struct nodeCDT * last;
};

typedef struct queueCDT * queue;

typedef struct nodeCDT * node;

queue new_command_queue();

node dequeue(queue queue);

node peek(queue queue);

void enqueue(queue queue, node node);

bool is_empty(queue queue);

void free_node(node node);

void destroy(queue queue);

