#pragma once

#include <stdint.h>

void memcopy(void * dest, const void * src, uint64_t n);

void memzero(void * dest, uint64_t n);
