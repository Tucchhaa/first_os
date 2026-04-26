#pragma once

#include <stdint.h>

void uart_sync_setup();

uint8_t uart_sync_get(void);
void uart_sync_get_bytes(uint8_t * buf, int n);
void uart_sync_put(uint8_t b);
void uart_sync_puts(const char * str);
void uart_sync_puts_variadic(const char * first, ...);
void uart_sync_getline(char * buf, int n);
