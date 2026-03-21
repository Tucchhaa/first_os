#include "uart.h"
#include "platform.h"

volatile uint8_t * _uart_base = 0;
volatile uint8_t * _uart_status = 0;

void uart_setup(uintptr_t base_addr) {
    _uart_base = (uint8_t *)base_addr;
    _uart_status = (uint8_t *)(base_addr + UART_STATUS_OFFSET);
}

// Transmit Holding Register Empty
static inline int uart_status_thre(void) {
    return !!(*_uart_status & (1u << 5));
}

// Data ready
static inline int uart_status_dr(void) {
    return !!(*_uart_status & (1u << 0));
}

uint8_t uart_get(void) {
    while(!uart_status_dr()) { }
    return *_uart_base;
}

void uart_get_bytes(uint8_t * buf, int n) {
    for(int i=0; i < n; i++) {
        buf[i] = uart_get();
    }
}

void uart_put(uint8_t b) {
    while(!uart_status_thre()) { }
    *_uart_base = b;
}

void uart_puts(const char * str) {
    for(int i=0; str[i] != '\0'; i++) {
        if (str[i] == '\n') uart_put('\r');
        uart_put(str[i]);
    }
}

void uart_getline(char * buf, int n) {
    int i=0;

    while(i < n - 1) {
        char c = (char)uart_get();

        uart_put(c);

        if (c == '\r' || c == '\n') {
            uart_put('\n');
            break;
        }

        buf[i++] = c;
    }

    buf[i] = '\0';
}
