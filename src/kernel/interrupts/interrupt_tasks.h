#pragma once

#include <stdint.h>

void interrupt_tasks_setup();

uint8_t interrupt_tasks_is_empty();

void interrupt_tasks_add(
    void (*handler)(void *), 
    void * arg, 
    uint8_t priority
);

void interrupt_tasks_execute();
