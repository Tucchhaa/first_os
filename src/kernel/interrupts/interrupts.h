#pragma once

#include <stdint.h>

struct trapframe;

extern uint8_t is_handling_interrupt;

void set_need_reschedule_cpu();

void interrupts_setup();

void interrupts_enable();
void interrupts_restore(uint8_t pie);
uint8_t interrupts_disable();

void interrupts_enable_external();
void interrupts_disable_external();

void interrupts_enable_timer();
void interrupts_disable_timer();

void interrupts_handler(struct trapframe * trapframe);
