#pragma once

#include <stdint.h>

extern uint8_t uart_ready;
extern uint32_t uart_irq;

void uart_setup();

void uart_irq_handler(void);

uint8_t uart_receive_buf_empty();

uint8_t uart_transmit_buf_full();

uint32_t uart_get_bytes(char * buf, uint32_t n);

uint32_t uart_put_bytes(const char * buf, uint32_t n);

void uart_put(char b);

void uart_puts(const char * str);

void uart_puts_variadic(const char * first, ...);

uint32_t uart_getline(char * buf, uint32_t n);
