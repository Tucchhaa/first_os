#include "uart.h"

#include <stdarg.h>

#include "_uart_regs.h"
#include "../fdt/fdt.h"
#include "../converters.h"
#include "../string.h"
#include "../kernel/task/task.h"
#include "../kernel/interrupts/plic.h"
#include "../kernel/interrupts/interrupt_control.h"
#include "../kernel/task/cpu_scheduler.h"

#define RX_RING_SIZE 512
#define TX_RING_SIZE 512

static struct uart_regs _uart_regs;

uint8_t uart_ready = 0;
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

    uart_ready = 1;
}

void uart_debug_puts(const char * s) {
    uint8_t pie = interrupts_disable();

    // drain TX ring first so output stays ordered
    while (tx_head != tx_tail) {
        while (!uart_status_thre(&_uart_regs)) { }
        *_uart_regs.base = tx_ring[tx_tail];
        tx_tail = (tx_tail + 1) % TX_RING_SIZE;
    }

    for (; *s; s++) {
        if (*s == '\n') {
            while (!uart_status_thre(&_uart_regs)) { }
            *_uart_regs.base = '\r';
        }

        while (!uart_status_thre(&_uart_regs)) { }
        *_uart_regs.base = *s;
    }

    interrupts_restore(pie);
}

void uart_irq_handler() {
    uint8_t has_received = 0;

    while (uart_status_dr(&_uart_regs)) {
        uint8_t c = *_uart_regs.base;
        uint32_t next = (rx_head + 1) % RX_RING_SIZE;

        if (next != rx_tail) {
            has_received = 1;
            rx_ring[rx_head] = c;
            rx_head = next;
        }
        // Overflow: drop byte.
    }

    if (has_received) {
        cpu_scheduler_fire(TASK_WAIT_UART_READ);
    }
    
    uint8_t has_transmitted = 0;
    
    while (uart_status_thre(&_uart_regs)) {
        if (tx_head == tx_tail) {
            // Ring drained: stop asking for THR-empty interrupts.
            *_uart_regs.ier &= ~UART_IER_THR_EMPTY;
            break;
        }

        has_transmitted = 1;
        *_uart_regs.base = tx_ring[tx_tail];
        tx_tail = (tx_tail + 1) % TX_RING_SIZE;
    }

    if (has_transmitted) {
        cpu_scheduler_fire(TASK_WAIT_UART_WRITE);
    }
}

uint8_t uart_receive_buf_empty() {
    return rx_head == rx_tail;
}

uint8_t uart_transmit_buf_full() {
    return (tx_head + 1) % TX_RING_SIZE == tx_tail;
}

static uint8_t _uart_get_core(char * c) {
    if (uart_receive_buf_empty()) {
        return 0;
    }

    *c = (char)rx_ring[rx_tail];
    rx_tail = (rx_tail + 1) % RX_RING_SIZE;

    return 1;
}

char uart_get() {
    char c;
    
    while (1) {
        uint8_t pie = interrupts_disable();
        uint8_t is_ring_drained = _uart_get_core(&c) == 0;

        if (is_ring_drained) {
            interrupts_restore(pie);
            cpu_scheduler_wait(TASK_WAIT_UART_READ);
            continue;
        }

        break;
    }

    return c;
}

uint32_t uart_get_bytes(char * buf, uint32_t n) {
    uint8_t pie = interrupts_disable();
    uint32_t i = 0;

    for(; i < n; i++) {
        if (_uart_get_core(&buf[i]) == 0) {
            break;
        }
    }

    interrupts_restore(pie);

    return i;
}

static uint8_t _uart_put_core(char c) {
    if (uart_transmit_buf_full()) {
        return 0;
    }

    tx_ring[tx_head] = (uint8_t)c;
    tx_head = (tx_head + 1) % TX_RING_SIZE;
    *_uart_regs.ier |= UART_IER_THR_EMPTY;

    return 1;
}

void uart_put(char c) {
    while (1) {
        uint8_t pie = interrupts_disable();
        uint8_t result = _uart_put_core(c);
        interrupts_restore(pie);

        if (result) {
            return;
        }

        cpu_scheduler_wait(TASK_WAIT_UART_WRITE);
    }
}

uint32_t uart_put_bytes(const char * buf, uint32_t count) {
    uint8_t pie = interrupts_disable();
    uint32_t i = 0;

    for (; i < count; i++) {
        uint8_t ring_drained = 0;

        // TODO: if only one slot left in the ring, it could be undef behavior 
        if (buf[i] == '\n') {
            ring_drained |= _uart_put_core('\r') == 0;
        }

        ring_drained |= _uart_put_core(buf[i]) == 0;

        if (ring_drained) {
            break;
        }
    }

    interrupts_restore(pie);
    return i;
}

void uart_puts(const char * buf) {
    uint32_t count = kstrlen(buf);
    uint32_t written_count = 0;

    while (1) {
        written_count += uart_put_bytes(buf + written_count, count - written_count);

        if (written_count == count) {
            break;
        }

        cpu_scheduler_wait(TASK_WAIT_UART_WRITE);
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

uint32_t uart_getline(char * buf, uint32_t n) {
    uint32_t i = 0;

    while (i < n - 1) {
        char c = uart_get();

        uart_put(c);

        if (c == '\r' || c == '\n') {
            uart_put('\n');
            break;
        }

        buf[i++] = c;
    }

    buf[i] = '\0';
    return i + 1;
}
