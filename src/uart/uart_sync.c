#include "uart_sync.h"

#include <stdarg.h>

#include "_uart_regs.h"
#include "../fdt/fdt.h"

static struct uart_regs _uart_regs;

void uart_sync_setup() {
    uintptr_t serial_node = fdt_node_addr_by_path("/soc/serial");
    
    _uart_regs = uart_get_regs(serial_node);

    uart_sync_puts("[KERNEL:UART] Done setting up\n");
}

uint8_t uart_sync_get(void) {
    while(!uart_status_dr(&_uart_regs)) { }
    return *_uart_regs.base;
}

void uart_sync_get_bytes(uint8_t * buf, int n) {
    for(int i=0; i < n; i++) {
        buf[i] = uart_sync_get();
    }
}

void uart_sync_put(uint8_t b) {
    while(!uart_status_thre(&_uart_regs)) { }
    *_uart_regs.base = b;
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
