#include "../uart.h"
#include "../sbi.h"
#include "../string.h"

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

            ltox(sbi_get_spec_version().value, buf);
            uart_puts("  OpenSBI specification version: 0x");
            uart_puts(buf);
            uart_puts("\n");

            ltox(sbi_get_impl_id().value, buf);
            uart_puts("  implementation ID: 0x");
            uart_puts(buf);
            uart_puts("\n");

            ltox(sbi_get_impl_version().value, buf);
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

void kmain(void) {
    cli();
}
