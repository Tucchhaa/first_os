#include "../platform.h"
#include "../uart.h"
#include "../string.h"
#include "../fdt.h"
#include "../utils.h"

uint64_t hartid;
uintptr_t fdt_addr;

static void receive_kernel_bin(int kernel_size) {
    const uintptr_t KERNEL_ADDR = KERNEL_LOAD_ADDR;
    uint32_t loaded_bytes_count = 0;

    while (loaded_bytes_count < kernel_size) {
        uint8_t byte;
        uart_get_bytes((uint8_t *)&byte, sizeof(byte));

        *(volatile uintptr_t *)(KERNEL_ADDR + loaded_bytes_count) = byte;
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

        // 'BOOT'`
        if (magic == 0x544F4F42) {
            uart_puts("Receiving kernel...\n");
            receive_kernel_bin(kernel_size);
        } else {
            uart_puts("Failed to receive kernel\n");
        }
    }
}

static void setup_uart() {
    uintptr_t soc_node = fdt_node_addr_by_path(fdt_addr, "/soc");
    struct fdt_property * soc_cell_address_cells_prop = fdt_property_at_addr(
        fdt_property_addr_by_name(fdt_addr, soc_node, "#address-cells")
    );

    uint32_t address_cells = be32_to_cpu(*(uint32_t *)(&soc_cell_address_cells_prop->data));

    uintptr_t soc_serial_node = fdt_node_addr_by_path(fdt_addr, "/soc/serial");
    struct fdt_property * serial_reg_prop = fdt_property_at_addr(
        fdt_property_addr_by_name(fdt_addr, soc_serial_node, "reg")
    );

    uintptr_t uart_base = address_cells == 1
        ? be32_to_cpu(*(uint32_t *)(&serial_reg_prop->data))
        : be64_to_cpu(*(uint64_t *)(&serial_reg_prop->data));

    uart_setup(uart_base);
}

void bootloader(uint64_t _hartid, uintptr_t _fdt_addr) {
    hartid = _hartid;
    fdt_addr = _fdt_addr;

    if (fdt_check_magic(fdt_addr) == 0) {
        return;
    }

    setup_uart();
    cli();
}
