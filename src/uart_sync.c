#include <stdarg.h>

#include "uart_sync.h"
#include "platform.h"
#include "fdt/fdt.h"

static volatile uint8_t * _uart_base = 0;
static volatile uint8_t * _uart_status = 0;

void uart_sync_setup() {
    uintptr_t uart_base_addr;
    uintptr_t soc_serial_node = fdt_node_addr_by_path("/soc/serial");
    
    fdt_reg_property(soc_serial_node, &uart_base_addr, (void*)0);
    
    _uart_base = (uint8_t *)uart_base_addr;
    _uart_status = (uint8_t *)(uart_base_addr + UART_STATUS_OFFSET);

    uart_sync_puts("[KERNEL:UART] Done setting up\n");
}

// Transmit Holding Register Empty
static inline int uart_status_thre(void) {
    return !!(*_uart_status & (1u << 5));
}

// Data ready
static inline int uart_status_dr(void) {
    return !!(*_uart_status & (1u << 0));
}

uint8_t uart_sync_get(void) {
    while(!uart_status_dr()) { }
    return *_uart_base;
}

void uart_sync_get_bytes(uint8_t * buf, int n) {
    for(int i=0; i < n; i++) {
        buf[i] = uart_sync_get();
    }
}

void uart_sync_put(uint8_t b) {
    while(!uart_status_thre()) { }
    *_uart_base = b;
}

void uart_sync_puts(const char * str) {
    for(int i=0; str[i] != '\0'; i++) {
        if (str[i] == '\n') uart_sync_put('\r');
        uart_sync_put(str[i]);
    }
}

void uart_sync_puts_variadic(const char * first, ...) {
    va_list args;
    va_start(args, first);

    const char * arg = first;

    while (arg != 0) {
        uart_sync_puts(arg);
        arg = va_arg(args, const char *);
    }

    va_end(args);
}

void uart_sync_getline(char * buf, int n) {
    int i=0;

    while(i < n - 1) {
        char c = (char)uart_sync_get();

        uart_sync_put(c);

        if (c == '\r' || c == '\n') {
            uart_sync_put('\n');
            break;
        }

        buf[i++] = c;
    }

    buf[i] = '\0';
}
