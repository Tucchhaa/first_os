#pragma once

#include <stdint.h>

struct trapframe;

extern uint8_t is_handling_interrupt;

void set_need_reschedule_cpu();

void interrupts_setup();

void interrupts_handler(struct trapframe * trapframe);
