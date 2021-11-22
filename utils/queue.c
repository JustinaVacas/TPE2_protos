#include <queue.h>

queue new_command_queue(){
    return calloc(1, sizeof(struct queueCDT));
}

node dequeue(queue queue) {
    if (is_empty(queue))
        return NULL;

    node aux = queue->first;
    queue->first = aux->next;
    queue->size--;
    if (queue->size == 0)
        queue->last = NULL;
    return aux;
}


void enqueue(queue queue, node node) {
    if (is_empty(queue)) {
        queue->first = node;
        queue->last = node;
    } else {
        queue->last->next = node;
        queue->last = node;
    }
    queue->size++;
}

bool is_empty(queue queue) {
    return queue->size == 0;
}

void free_node(node node){
    if(node == NULL)
        return;

    free_node(node->next);
    free(node);
}

void destroy(queue queue){
    free_node(queue->first);
    free(queue);
}

node peek(queue queue) {
    return queue->first;
}
