#pragma once

#include <stdint.h>

void interrupts_enable();
void interrupts_restore(uint8_t pie);
uint8_t interrupts_disable();

void interrupts_enable_external();
void interrupts_disable_external();

void interrupts_enable_timer();
void interrupts_disable_timer();
