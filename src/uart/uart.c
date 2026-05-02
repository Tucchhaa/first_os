#include "uart.h"

#include <stdarg.h>

#include "_uart_regs.h"
#include "../fdt/fdt.h"
#include "../converters.h"
#include "../string.h"
#include "../kernel/interrupts/plic.h"
#include "../kernel/interrupts/interrupts.h"

#define RX_RING_SIZE 512
#define TX_RING_SIZE 512

static struct uart_regs _uart_regs;

uint32_t uart_irq = 0;

static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_head;
static volatile uint32_t rx_tail;

static volatile uint8_t tx_ring[TX_RING_SIZE];
static volatile uint32_t tx_head;
static volatile uint32_t tx_tail;

void uart_setup() {
    uintptr_t serial_node = fdt_node_addr_by_path("/soc/serial");
    
    _uart_regs = uart_get_regs(serial_node);

    rx_head = rx_tail = 0;
    tx_head = tx_tail = 0;

    struct fdt_property * irq_prop = fdt_property_by_name(serial_node, "interrupts");

    if (irq_prop != 0) {
        *_uart_regs.ier |= UART_IER_RX_AVAILABLE;
        *_uart_regs.mcr |= UART_MCR_OUT2;
        uart_irq = be32_to_cpu(*(uint32_t *)(&irq_prop->data));
        plic_enable_irq(uart_irq, 1);
    }
}

void uart_irq_handler() {
    while (uart_status_dr(&_uart_regs)) {
        uint8_t c = *_uart_regs.base;
        uint32_t next = (rx_head + 1) % RX_RING_SIZE;

        if (next != rx_tail) {
            rx_ring[rx_head] = c;
            rx_head = next;
        }
        // Overflow: drop byte.
    }

    while (uart_status_thre(&_uart_regs)) {
        if (tx_head == tx_tail) {
            // Ring drained: stop asking for THR-empty interrupts.
            *_uart_regs.ier &= ~UART_IER_THR_EMPTY;
            break;
        }

        *_uart_regs.base = tx_ring[tx_tail];
        tx_tail = (tx_tail + 1) % TX_RING_SIZE;
    }
}

uint8_t uart_get() {
    while (rx_head == rx_tail) {
        asm volatile("wfi");
    }

    uint8_t c = rx_ring[rx_tail];
    rx_tail = (rx_tail + 1) % RX_RING_SIZE;

    return c;
}

void uart_put(uint8_t b) {
    while (1) {
        uint32_t next = (tx_head + 1) % TX_RING_SIZE;

        if (next == tx_tail) {
            asm volatile("wfi");
            continue;
        }

        // TODO: it's possible to disable interrupts per string, not per byte
        uint8_t pie = interrupts_disable();

        tx_ring[tx_head] = b;
        tx_head = next;
        *_uart_regs.ier |= UART_IER_THR_EMPTY;

        interrupts_restore(pie);

        return;
    }
}

void uart_puts(const char * str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') uart_put('\r');
        uart_put(str[i]);
    }
}

void uart_puts_variadic(const char * first, ...) {
    va_list args;
    va_start(args, first);

    const char * arg = first;

    while (arg != 0) {
        uart_puts(arg);
        arg = va_arg(args, const char *);
    }

    va_end(args);
}


void uart_getline(char * buf, int n) {
    int i = 0;

    while (i < n - 1) {
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
