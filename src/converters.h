#pragma once

#include <stdint.h>

uint32_t be32_to_cpu(uint32_t x);

uint64_t be64_to_cpu(uint64_t x);

void i64tox(int64_t x, char * const buf);
void i32tox(int32_t x, char * const buf);
void i8tox(int8_t x, char * const buf);

uint32_t xtoi32(char * const buf);

void itoa(int64_t, char * const buf);
uint64_t atoi(char * const buf);
