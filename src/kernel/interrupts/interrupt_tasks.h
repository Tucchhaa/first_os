#pragma once

#include <stdint.h>

void interrupt_tasks_setup();

void interrupt_tasks_add(
    void (*handler)(void *), 
    void * arg, 
    uint8_t priority
);

uint8_t interrupt_tasks_execute();
