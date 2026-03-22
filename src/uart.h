#pragma once

#include <stdint.h>

void uart_setup(uintptr_t base_addr);

uint8_t uart_get(void);
void uart_get_bytes(uint8_t * buf, int n);
void uart_put(uint8_t b);
void uart_puts(const char * str);
void uart_puts_variadic(const char * first, ...);
void uart_getline(char * buf, int n);
