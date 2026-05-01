#include "uart.h"

#include <stdarg.h>

#include "../../platform.h"
#include "../../fdt/fdt.h"
#include "../../converters.h"
#include "../../uart_sync.h"
#include "../../string.h"
#include "../interrupts/plic.h"
#include "../interrupts/interrupts.h"

#define RX_RING_SIZE 512
#define TX_RING_SIZE 512

static const uint8_t UART_IER_RX_AVAILABLE = (1u << 0);
static const uint8_t UART_IER_THR_EMPTY = (1u << 1);

static const uint8_t UART_MCR_OUT2 = (1u << 3);

static volatile uint8_t * _uart_base = 0;
static volatile uint8_t * _uart_status = 0;
static volatile uint8_t * _uart_ier = 0;
static volatile uint8_t * _uart_mcr = 0;

uint32_t uart_irq = 0;

static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_head;
static volatile uint32_t rx_tail;

static volatile uint8_t tx_ring[TX_RING_SIZE];
static volatile uint32_t tx_head;
static volatile uint32_t tx_tail;

// Transmit Holding Register Empty
static inline uint8_t uart_status_thre(void) {
    return !!(*_uart_status & (1u << 5));
}

// Data ready
static inline uint8_t uart_status_dr(void) {
    return !!(*_uart_status & (1u << 0));
}

void uart_setup() {
    uintptr_t uart_addr;
    uintptr_t soc_serial_node = fdt_node_addr_by_path("/soc/serial");
    
    fdt_reg_property(soc_serial_node, &uart_addr, (void*)0);
    
    _uart_base = (uint8_t *)uart_addr;
    _uart_status = (uint8_t *)(uart_addr + UART_STATUS_OFFSET);
    _uart_ier = (uint8_t *)(uart_addr + UART_IER_OFFSET);
    _uart_mcr = (uint8_t *)(uart_addr + UART_MCR_OFFSET);

    rx_head = rx_tail = 0;
    tx_head = tx_tail = 0;

    struct fdt_property * irq_prop = fdt_property_by_name(soc_serial_node, "interrupts");

    if (irq_prop != 0) {
        *_uart_ier |= UART_IER_RX_AVAILABLE;
        *_uart_mcr |= UART_MCR_OUT2;
        uart_irq = be32_to_cpu(*(uint32_t *)(&irq_prop->data));
        // uart_irq = 38;
        plic_enable_irq(uart_irq, 1);
    }
}

void uart_irq_handler() {
    while (uart_status_dr()) {
        uint8_t c = *_uart_base;
        uint32_t next = (rx_head + 1) % RX_RING_SIZE;

        if (next != rx_tail) {
            rx_ring[rx_head] = c;
            rx_head = next;
        }
        // Overflow: drop byte.
    }

    while (uart_status_thre()) {
        if (tx_head == tx_tail) {
            // Ring drained: stop asking for THR-empty interrupts.
            *_uart_ier &= ~UART_IER_THR_EMPTY;
            break;
        }

        *_uart_base = tx_ring[tx_tail];
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
        uint8_t prev = interrupts_disable();

        tx_ring[tx_head] = b;
        tx_head = next;
        *_uart_ier |= UART_IER_THR_EMPTY;

        if (prev) {
            interrupts_enable();
        }

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
