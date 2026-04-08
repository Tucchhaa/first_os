#pragma once

#include <stdint.h>

void dynamic_allocator_init(void);

void * allocate(uint32_t size);

void free(void * addr);
