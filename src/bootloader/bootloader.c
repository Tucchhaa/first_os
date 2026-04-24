#include "../platform.h"
#include "../uart.h"
#include "../string.h"
#include "../fdt/fdt.h"
#include "../utils.h"

uint64_t hartid;

static void receive_kernel_bin(int kernel_size) {
    const uintptr_t KERNEL_ADDR = KERNEL_LOAD_ADDR;
    uint32_t loaded_bytes_count = 0;

    while (loaded_bytes_count < kernel_size) {
        uint8_t byte;
        uart_get_bytes((uint8_t *)&byte, sizeof(byte));

        *(volatile uint8_t *)(KERNEL_ADDR + loaded_bytes_count) = byte;
        loaded_bytes_count += 1;
    }

    // make sure that all writes are finished before jumping to kernel
    asm volatile ("fence"   ::: "memory");
    asm volatile ("fence.i" ::: "memory");

    ((void (*)(uint64_t, uintptr_t))KERNEL_ADDR)(hartid, fdt_addr);
}

static void cli(void) {
    while(1) {
        uart_puts("Waiting for kernel.bin\n");

        uint32_t magic, kernel_size;
        uart_get_bytes((uint8_t *)&magic, sizeof(magic));
        uart_get_bytes((uint8_t *)&kernel_size, sizeof(kernel_size));

        // 'BOOT'
        if (magic == 0x544F4F42) {
            uart_puts("Receiving kernel...\n");
            receive_kernel_bin(kernel_size);
        } else {
            uart_puts("Failed to receive kernel\n");
        }
    }
}

static void setup_uart() {
    uintptr_t soc_serial_node = fdt_node_addr_by_path("/soc/serial");
    uint64_t uart_base;
    fdt_read_reg_property(soc_serial_node, &uart_base, (void*)0);
    uart_setup(uart_base);
}

static void setup_fallback_uart() {
    uart_setup(UART_BASE);
}

void bootloader(uint64_t _hartid, uintptr_t _fdt_addr) {
    hartid = _hartid;

    // setup_fallback_uart();

    if (fdt_setup(_fdt_addr) == 0) {
        return;
    }

    setup_uart();
    cli();
}
