#pragma once

#include <stdint.h>

uint32_t kstrlen(const char * s);

uint8_t streqln(const char * a, const char * b, uint32_t len);
uint8_t streql(const char * a, const char * b);

uint32_t strtoken(const char * s, const char * token, uint32_t offset);

void strslice(const char * s, char * buf, uint32_t index, uint32_t n);

void i64tox(int64_t x, char * const buf);
void i32tox(int32_t x, char * const buf);
void i8tox(int8_t x, char * const buf);
