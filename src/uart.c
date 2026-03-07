#include <stdint.h>
#include "uart.h"
#include "platform.h"

volatile uint8_t * const uart_trasm = (volatile uint8_t *)(UART_BASE);
volatile uint8_t * const uart_status = (volatile uint8_t *)(UART_BASE + UART_STATUS_OFFSET);

// Transmit Holding Register Empty
static inline int uart_status_thre(void) {
    return !!(*uart_status & (1u << 5));
}

// Data ready
static inline int uart_status_dr(void) {
    return !!(*uart_status & (1u << 0));
}

void uart_putc(char c) {
    while(!uart_status_thre()) { }
    *uart_trasm = (uint8_t)c;
}

void uart_puts(const char * str) {
    for(int i=0; str[i] != '\0'; i++) {
        if (str[i] == '\n') uart_putc('\r');
        uart_putc(str[i]);
    }
}

char uart_getc(void) {
    while(!uart_status_dr()) { }
    char c = *uart_trasm;
    uart_putc(c);
    return c;
}

void uart_getline(char * buf, int n) {
    int i=0;

    while(i < n - 1) {
        char c = uart_getc();

        if (c == '\r' || c == '\n') {
            uart_putc('\n');
            break;
        }

        buf[i++] = c;
    }

    buf[i] = '\0';
}
