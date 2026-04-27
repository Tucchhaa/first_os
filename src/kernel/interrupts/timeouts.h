#pragma once

#include <stdint.h>

extern uint32_t cpu_frequency;

void timeouts_setup();

void set_timeout(void (*callback)(void *), void * arg, uint64_t timeout_ms);

void timeouts_interrupt_handler();
