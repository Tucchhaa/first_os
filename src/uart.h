#include <stdint.h>

uint8_t uart_get(void);
void uart_get_bytes(uint8_t * buf, int n);
void uart_put(uint8_t b);
void uart_puts(const char * str);
void uart_getline(char * buf, int n);
