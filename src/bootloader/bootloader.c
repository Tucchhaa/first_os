#include "../platform.h"
#include "../uart/uart_sync.h"
#include "../fdt/fdt.h"

uint64_t hartid;

static void receive_kernel_bin(int kernel_size) {
    const uintptr_t KERNEL_ADDR = KERNEL_PHYSICAL_ADDR;
    uint32_t loaded_bytes_count = 0;

    while (loaded_bytes_count < kernel_size) {
        uint8_t byte;
        uart_sync_get_bytes((uint8_t *)&byte, sizeof(byte));

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
        uart_sync_puts("Waiting for kernel.bin\n");

        uint32_t magic, kernel_size;
        uart_sync_get_bytes((uint8_t *)&magic, sizeof(magic));
        uart_sync_get_bytes((uint8_t *)&kernel_size, sizeof(kernel_size));

        // 'BOOT'
        if (magic == 0x544F4F42) {
            uart_sync_puts("Receiving kernel...\n");
            receive_kernel_bin(kernel_size);
        } else {
            uart_sync_puts("Failed to receive kernel\n");
        }
    }
}

void bootloader(uint64_t _hartid, uintptr_t _fdt_addr) {
    hartid = _hartid;

    if (fdt_setup(_fdt_addr)) {
        return;
    }

    uart_sync_setup();
    cli();
}
