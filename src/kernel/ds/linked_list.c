#include "linked_list.h"

struct linked_list * linked_list_init(void * addr) {
    struct linked_list * list = (struct linked_list *)addr;
    list->head = (void *)0;
    list->tail = (void *)0;

    return list;
}

void linked_list_insert(struct linked_list * list, struct linked_list_node * node) {
    if (list->head == 0) {
        list->head = node;
        list->tail = node;
        node->next = (void *)0;
        node->prev = (void *)0;
        return;
    }

    node->prev = list->tail;
    node->next = (void *)0;

    list->tail->next = node;
    list->tail = node;
}

void linked_list_remove(struct linked_list * list, struct linked_list_node * node) {
    if (node->next == 0) {
        list->tail = node->prev;
    } else {
        node->next->prev = node->prev;
    }

    if (node->prev == 0) {
        list->head = node->next;
    } else {
        node->prev->next = node->next;
    }

    node->next = (void *)0;
    node->prev = (void *)0;
}
