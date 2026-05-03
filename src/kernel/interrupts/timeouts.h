#pragma once

#include <stdint.h>

extern uint64_t cpu_frequency;

void timeouts_setup();

void timeouts_postpone();

uint32_t set_timeout(void (*callback)(void *), void * arg, uint64_t timeout_ms);

void clear_timeout(uint32_t timeout_id);

void timeouts_interrupt_handler(void *);
