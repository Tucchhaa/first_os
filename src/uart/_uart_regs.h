#pragma once

#include <stdint.h>

static const uint8_t UART_IER_RX_AVAILABLE = (1u << 0);
static const uint8_t UART_IER_THR_EMPTY = (1u << 1);

static const uint8_t UART_MCR_OUT2 = (1u << 3);

struct uart_regs {
    volatile uint8_t * base;
    volatile uint8_t * status;
    volatile uint8_t * ier; // Interrupt Enable Register
    volatile uint8_t * mcr; // Modem Control Register
};

struct uart_regs uart_get_regs(uintptr_t serial_node);

// Transmit Holding Register Empty
static inline uint8_t uart_status_thre(struct uart_regs * uart_regs) {
    return !!(*(uart_regs->status) & (1u << 5));
}

// Data ready
static inline uint8_t uart_status_dr(struct uart_regs * uart_regs) {
    return !!(*(uart_regs->status) & (1u << 0));
}
