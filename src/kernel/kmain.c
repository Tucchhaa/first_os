#include "../uart.h"
#include "../sbi.h"
#include "../string.h"
#include "../fdt.h"
#include "../utils.h"

static void cli(void) {
    const int command_max_size = 100;
    char command[command_max_size];

    while(1) {
        uart_puts("sh> ");
        uart_getline(command, command_max_size);

        if (streql(command, "hello")) {
            uart_puts("Hello World!\n");
        } else if (streql(command, "help")) {
            uart_puts("Available commands:\n");
            uart_puts("  help - show all commands.\n");
            uart_puts("  hello - print Hello world.\n");
            uart_puts("  info - print system info.\n");
        } else if (streql(command, "info")) {
            char buf[20];

            uart_puts("System information:\n");

            if (sbi_get_spec_version().error) {
                uart_puts("  error occured\n");
                continue;
            }

            i64tox(sbi_get_spec_version().value, buf);
            uart_puts("  OpenSBI specification version: 0x");
            uart_puts(buf);
            uart_puts("\n");

            i64tox(sbi_get_impl_id().value, buf);
            uart_puts("  implementation ID: 0x");
            uart_puts(buf);
            uart_puts("\n");

            i64tox(sbi_get_impl_version().value, buf);
            uart_puts("  implementation version: 0x");
            uart_puts(buf);
            uart_puts("\n");
        } else {
            uart_puts("Unknown command: ");
            uart_puts(command);
            uart_puts("\n");
        }
    }
}

uintptr_t fdt_addr;

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


void kmain(uint64_t _hartid, uintptr_t _fdt_addr) {
    fdt_addr = _fdt_addr;

    setup_uart();
    cli();
}
