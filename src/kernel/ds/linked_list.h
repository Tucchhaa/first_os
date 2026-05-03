#pragma once

#include <stdint.h>

struct linked_list_node {
    struct linked_list_node * prev;
    struct linked_list_node * next;
};

struct linked_list {
    struct linked_list_node * head;
    struct linked_list_node * tail;
};

struct linked_list * linked_list_init(void * addr);

void linked_list_insert(struct linked_list * list, struct linked_list_node * node);

void linked_list_insert_before(
    struct linked_list * list, 
    struct linked_list_node * node,
    struct linked_list_node * before_node
);

void linked_list_remove(struct linked_list * list, struct linked_list_node * node);
