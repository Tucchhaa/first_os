#include "../platform.h"
#include "../uart.h"
#include "../string.h"

static void receive_kernel_bin(int kernel_size) {
    const uintptr_t KERNEL_ADDR = KERNEL_LOAD_ADDR;
    int loaded_bytes_count = 0;

    while (loaded_bytes_count < kernel_size) {
        uint8_t byte;
        uart_get_bytes((uint8_t *)&byte, sizeof(byte));

        *(volatile uintptr_t *)(KERNEL_ADDR + loaded_bytes_count) = byte;
        loaded_bytes_count += 1;
    }

    // make sure that CPU committed all writes are finished before jumping to kernel
    asm volatile ("fence"   ::: "memory");
    asm volatile ("fence.i" ::: "memory");

    ((void (*)(void))KERNEL_ADDR)();
}

void bootloader(void) {
    const int command_max_size = 100;
    char command[command_max_size];

    while(1) {
        uart_puts("bootloader> ");
        uart_getline(command, command_max_size);

        if (streql(command, "help")) {
            uart_puts("To load kernel run \'load\' command and send kernel.bin over UART\n");
        } else if (streql(command, "load")) {
            uart_puts("Waiting for kernel.bin\n");

            int magic, kernel_size;
            uart_get_bytes((uint8_t *)&magic, sizeof(magic));
            uart_get_bytes((uint8_t *)&kernel_size, sizeof(kernel_size));

            // 'BOOT'
            if (magic == 0x544F4F42) {
                uart_puts("Receiving kernel...\n");
                receive_kernel_bin(kernel_size);
            } else {
                uart_puts("Failed to receive kernel\n");
            }
        } else {
            uart_puts("Unknown command\n");
        }
    }
}
