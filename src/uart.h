#pragma once

#include <stdint.h>
#include <stdarg.h>

void uart_setup(uintptr_t base_addr);

uint8_t uart_get(void);
void uart_get_bytes(uint8_t * buf, int n);
void uart_put(uint8_t b);
void uart_puts(const char * str);
void uart_puts_variadic(const char * first, ...);
void uart_getline(char * buf, int n);

void async_uart_setup(void);
uint8_t async_uart_get(void);
void async_uart_get_bytes(uint8_t * buf, int n);
void async_uart_put(uint8_t b);
void async_uart_puts(const char * str);
void async_uart_puts_vlist(const char * first, va_list args);
void async_uart_puts_variadic(const char * first, ...);
void async_uart_getline(char * buf, int n);

void uart_irq_handler(void);
