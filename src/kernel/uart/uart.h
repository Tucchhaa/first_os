#pragma once

#include <stdint.h>

extern uint32_t uart_irq;

void uart_setup();

void uart_irq_handler(void);

uint8_t uart_get();

void uart_put(uint8_t b);

void uart_puts(const char * str);

void uart_puts_variadic(const char * first, ...);

void uart_getline(char * buf, int n);
