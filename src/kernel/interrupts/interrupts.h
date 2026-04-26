#pragma once

#include <stdint.h>

void interrupts_setup();

void interrupts_enable();
uint64_t interrupts_disable();

void interrupts_enable_external();
void interrupts_disable_external();

void interrupts_enable_timer();
void interrupts_disable_timer();

void interrupts_enter_umode(uintptr_t proc_addr);

void interrupts_handler();
