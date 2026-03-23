#include "../uart.h"
#include "../sbi.h"
#include "../string.h"
#include "../fdt.h"
#include "../utils.h"
#include "initrd.h"

uintptr_t fdt_addr;
uintptr_t initrd_start_addr;
uintptr_t initrd_end_addr;

static void setup_uart(void);
static void setup_initrd(void);

static void command_info(void);
static void command_testfdt(void);
static void command_ls(void);
static void command_cat(const char * command);

void kmain(uint64_t _hartid, uintptr_t _fdt_addr) {
    fdt_addr = _fdt_addr;

    setup_uart();
    setup_initrd();
    uart_puts("\n");
    
    const int command_max_size = 100;
    char command[command_max_size];

    while(1) {
        uart_puts("sh> ");
        uart_getline(command, command_max_size);

        if (streql(command, "help")) {
            uart_puts("Available commands:\n");
            uart_puts("  help - show all commands\n");
            uart_puts("  info - print system info\n");
            uart_puts("  ls - print file system.\n");
            uart_puts("  cat <filepath> - print contents of a file.\n");
            uart_puts("  testfdt - test fdt parser.\n");
        } 
        else if (streql(command, "info")) {
            command_info();
        }
        else if (streql(command, "testfdt")) {
            command_testfdt();
        }
        else if (streql(command, "ls")) {
            command_ls();
        }
        else if (streqln(command, "cat ", 4)) {
            command_cat(command);
        }
        else {
            uart_puts("Unknown command\n");
        }
    }
}

static void setup_uart(void) {
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

    uart_puts("[KERNEL] UART configuration done\n\n");
}

static void setup_initrd(void) {
    uart_puts("[KERNEL] Setting up initrd\n");

    uintptr_t chosen_node = fdt_node_addr_by_path(fdt_addr, "/chosen");
    struct fdt_property * initrd_start_prop = fdt_property_at_addr(
        fdt_property_addr_by_name(fdt_addr, chosen_node, "linux,initrd-start")
    );
    struct fdt_property * initrd_end_prop = fdt_property_at_addr(
        fdt_property_addr_by_name(fdt_addr, chosen_node, "linux,initrd-end")
    );

    if (initrd_start_prop == 0 || initrd_end_prop == 0) {
        uart_puts("[KERNEL] initrd-start or initrd_end prop is not found\n");
        return;
    }

    initrd_start_addr = be64_to_cpu(*(uint64_t *)(&initrd_start_prop->data));
    initrd_end_addr = be64_to_cpu(*(uint64_t *)(&initrd_end_prop->data));

    char buf[40];
    i64tox(initrd_start_addr, buf);
    uart_puts_variadic("[KERNEL] initrd-start addr: 0x", buf, "\n", 0);

    i64tox(initrd_end_addr, buf);
    uart_puts_variadic("[KERNEL] initrd-end addr: 0x", buf, "\n", 0);

    if (initrd_check_magic(initrd_start_addr)) {
        uart_puts("[KERNEL] initrd magic is correct\n");
    } else {
        uart_puts("[KERNEL] initrd magic is not correct\n");
    }
}

static void command_info(void) {
    char buf[20];

    uart_puts("System information:\n");

    if (sbi_get_spec_version().error) {
        uart_puts("  error occured\n");
        return;
    }

    i64tox(sbi_get_spec_version().value, buf);
    uart_puts_variadic("  OpenSBI specification version: 0x", buf, "\n", 0);

    i64tox(sbi_get_impl_id().value, buf);
    uart_puts_variadic("  implementation ID: 0x", buf, "\n", 0);

    i64tox(sbi_get_impl_version().value, buf);
    uart_puts_variadic("  implementation version: 0x", buf, "\n", 0);
    
    uart_puts("\n");
}

static void command_testfdt(void) {
    // interrupt-controller node
    uintptr_t interrupt_controller_node = fdt_node_addr_by_path(
        fdt_addr, "/cpus/cpu@0/interrupt-controller"
    );
    struct fdt_property * compatible_prop = fdt_property_at_addr(
        fdt_property_addr_by_name(fdt_addr, interrupt_controller_node, "compatible")
    );

    if (compatible_prop == 0) {
        uart_puts("/cpus/cpu@0/interrupt-controller[compatible] - not found\n");
    } else {
        char buf[40];
        i32toa(be32_to_cpu(compatible_prop->len), buf);
        uart_puts_variadic("property data len: ", buf, "\n\n", 0);

        uart_puts("compatible: ");

        uint32_t i = 0;

        while (i < be32_to_cpu(compatible_prop->len)) {
            uart_put(((const char *)&compatible_prop->data)[i]);
            i += 1;
        }
        uart_puts("\n");
    }

    // memory node
    uintptr_t root_node = fdt_node_addr_by_path(fdt_addr, "/");
    struct fdt_property * address_cells_prop = fdt_property_at_addr(
        fdt_property_addr_by_name(fdt_addr, root_node, "#address-cells")
    );
    uint32_t address_cells = be32_to_cpu(*(uint32_t *)(&address_cells_prop->data));

    struct fdt_property * size_cells_prop = fdt_property_at_addr(
        fdt_property_addr_by_name(fdt_addr, root_node, "#size-cells")
    );
    uint32_t size_cells = be32_to_cpu(*(uint32_t *)(&size_cells_prop->data));

    uintptr_t memory_node = fdt_node_addr_by_path(
        fdt_addr, "/memory"
    );

    struct fdt_property * reg_prop = fdt_property_at_addr(
        fdt_property_addr_by_name(fdt_addr, memory_node, "reg")
    );

    if (reg_prop == 0) {
        uart_puts("/memory[reg] - not found\n");
    } else {
        uint64_t memory_base = address_cells == 1
            ? be32_to_cpu(*(uint32_t *)(&reg_prop->data))
            : be64_to_cpu(*(uint64_t *)(&reg_prop->data));

        uintptr_t size_addr = (uintptr_t)&reg_prop->data + address_cells * sizeof(uint32_t);

        uint64_t memory_size = address_cells == 1
            ? be32_to_cpu(*(uint32_t *)size_addr)
            : be64_to_cpu(*(uint64_t *)size_addr);

        char buf1[40], buf2[40];
        i64tox(memory_base, buf1);
        i64tox(memory_size, buf2);

        uart_puts_variadic("memory: base=0x", buf1, " size=0x", buf2, "\n", 0);
    }
}

static void command_ls(void) {
    if (initrd_check_magic(initrd_start_addr) == 0) {
        uart_puts("initrd was not found\n");
        return;
    }

    // show files count
    uintptr_t file_addr = initrd_start_addr;
    uint32_t files_count = 0;

    while (file_addr != 0) {
        files_count += 1;
        file_addr = initrd_get_next_file_addr(file_addr);
    }

    char buf[40];
    i32toa(files_count, buf);
    uart_puts_variadic("Total ", buf, " files.\n", 0);

    // show all files
    file_addr = initrd_start_addr;

    while (file_addr != 0) {
        uint32_t path_size;
        const char * filepath = initrd_get_filepath(file_addr, &path_size);

        uint32_t file_data_size;
        initrd_get_filedata(file_addr, &file_data_size);

        i32toa(file_data_size, buf);
        uart_puts(buf);

        int32_t space_count = 8 - kstrlen(buf);

        while (space_count) {
            uart_puts(" ");
            space_count -= 1;
        }

        for(int i=0; i < path_size; i++) {
            uart_put(filepath[i]);
        }
        uart_puts("\n");

        file_addr = initrd_get_next_file_addr(file_addr);
    }

    uart_puts("\n");
}

static void command_cat(const char * command) {
    const char * file_name = &command[4];
    uint32_t file_name_len = kstrlen(file_name);

    uintptr_t file_addr = initrd_start_addr;

    while (file_addr != 0) {
        uint32_t path_size;
        const char * filepath = initrd_get_filepath(file_addr, &path_size);

        if (streqln(file_name, filepath, file_name_len)) {
            uint32_t data_size;
            uintptr_t data = initrd_get_filedata(file_addr, &data_size);

            for (int i=0; i < data_size; i++) {
                char c = ((char *)data)[i];
                
                if (c == '\n') { uart_put('\r'); }
                uart_put(c);
            }

            uart_puts("\n");
            return;
        }

        file_addr = initrd_get_next_file_addr(file_addr);
    }

    uart_puts("File not found\n\n");
}
