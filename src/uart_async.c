#include <stdarg.h>

#include "uart.h"
#include "platform.h"
#include "kernel/plic/plic.h"

#define UART_IER_RX_AVAILABLE (1u << 0)
#define UART_IER_THR_EMPTY    (1u << 1)

#define RX_RING_SIZE 256
#define TX_RING_SIZE 256

extern volatile uint8_t * _uart_base;
extern volatile uint8_t * _uart_ier;

int uart_status_thre(void);
int uart_status_dr(void);

static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_head;
static volatile uint32_t rx_tail;

static volatile uint8_t tx_ring[TX_RING_SIZE];
static volatile uint32_t tx_head;
static volatile uint32_t tx_tail;

// Clear sstatus.SIE and return the previous value so the caller can
// restore it. Used to protect the tiny producer-side critical section
// that is shared with the UART ISR (tx ring + IER bit toggle).
static inline uint64_t disable_sie(void) {
    uint64_t prev;
    asm volatile("csrrc %0, sstatus, %1" : "=r"(prev) : "r"(1u << 1) : "memory");
    return prev;
}

static inline void restore_sie(uint64_t prev) {
    if (prev & (1u << 1)) {
        asm volatile("csrs sstatus, %0" :: "r"(1u << 1) : "memory");
    }
}

void async_uart_setup(void) {
    rx_head = rx_tail = 0;
    tx_head = tx_tail = 0;

    // Enable Received Data Available interrupt. THR-Empty is toggled on
    // demand so an empty TX ring doesn't storm us with interrupts.
    *_uart_ier = UART_IER_RX_AVAILABLE;

    plic_init();
    plic_enable_irq(UART_IRQ, 1);

    // Enable S-mode external interrupts (SEIE)
    asm volatile("csrs sie, %0" :: "r"(1u << 9) : "memory");
}

void uart_irq_handler(void) {
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

uint8_t async_uart_get(void) {
    while (rx_head == rx_tail) {
        asm volatile("wfi");
    }
    uint8_t c = rx_ring[rx_tail];
    rx_tail = (rx_tail + 1) % RX_RING_SIZE;
    return c;
}

void async_uart_get_bytes(uint8_t * buf, int n) {
    for (int i = 0; i < n; i++) {
        buf[i] = async_uart_get();
    }
}

void async_uart_put(uint8_t b) {
    while (1) {
        uint32_t next = (tx_head + 1) % TX_RING_SIZE;

        if (next == tx_tail) {
            asm volatile("wfi");
            continue;
        }

        uint64_t prev = disable_sie();

        tx_ring[tx_head] = b;
        tx_head = next;
        *_uart_ier |= UART_IER_THR_EMPTY;

        restore_sie(prev);
        return;
    }
}

void async_uart_puts(const char * str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') async_uart_put('\r');
        async_uart_put(str[i]);
    }
}

void async_uart_puts_vlist(const char * first, va_list args) {
    const char * arg = first;
    while (arg != 0) {
        async_uart_puts(arg);
        arg = va_arg(args, const char *);
    }
}

void async_uart_puts_variadic(const char * first, ...) {
    va_list args;
    va_start(args, first);

    const char * arg = first;

    while (arg != 0) {
        async_uart_puts(arg);
        arg = va_arg(args, const char *);
    }

    va_end(args);
}

void async_uart_getline(char * buf, int n) {
    int i = 0;

    while (i < n - 1) {
        char c = (char)async_uart_get();

        async_uart_put(c);

        if (c == '\r' || c == '\n') {
            async_uart_put('\n');
            break;
        }

        buf[i++] = c;
    }

    buf[i] = '\0';
}
