#pragma once

#include <stdint.h>

struct trapframe;

extern uint8_t is_handling_interrupt;

void set_need_reschedule_cpu();

void interrupt_setup();

void interrupt_handler(struct trapframe * trapframe);
