#include "../uart.h"
#include "../sbi.h"
#include "../string.h"
#include "../fdt/fdt.h"
#include "../utils.h"
#include "interrupts/interrupts.h"
#include "initrd/initrd.h"
#include "mm/setup.h"
#include "mm/page_allocator.h"
#include "mm/dynamic_allocator.h"

static void command_info(void);
static void command_ls(void);
static void command_cat(const char * command);
static void command_exec(void);

/*
TODO:
- support multiple memory regions
- clean up the kmain.c
- optimize page allocator
- use single linked list
- implement self relocating bootloader
*/
void kmain(uint64_t _hartid, uintptr_t _fdt_addr) {
    if (fdt_setup(_fdt_addr) == 0) {
        return;
    }

    uart_setup();
    uart_puts("[KERNEL] UART was set uo\n\n");

    uart_puts("[KERNEL:INITRD] Setting up...\n");
    if (initrd_setup()) {
        char buf[40];
        i64tox(initrd_start_addr, buf);
        uart_puts_variadic("[KERNEL:INITRD] initrd-start addr: 0x", buf, "\n", 0);

        i64tox(initrd_end_addr, buf);
        uart_puts_variadic("[KERNEL:INITRD] initrd-end addr: 0x", buf, "\n", 0);
    } else {
        uart_puts("[KERNEL:INITRD] Could not setup initrd. Probably initramfs is not passed\n");
    }
    uart_puts("\n");

    memory_setup();
    interrupts_setup();
    
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
            uart_puts("  exec - execute user program.\n");
        } 
        else if (streql(command, "info")) {
            command_info();
        }
        else if (streql(command, "ls")) {
            command_ls();
        }
        else if (streqln(command, "cat ", 4)) {
            command_cat(command);
        }
        else if (streql(command, "exec")) {
            command_exec();
        }
        else {
            uart_puts("Unknown command\n");
        }
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
    itoa(files_count, buf);
    uart_puts_variadic("Total ", buf, " files.\n", 0);

    // show all files
    file_addr = initrd_start_addr;

    while (file_addr != 0) {
        uint32_t path_size;
        const char * filepath = initrd_get_filepath(file_addr, &path_size);

        uint32_t file_data_size;
        initrd_get_filedata(file_addr, &file_data_size);

        itoa(file_data_size, buf);
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

static void command_exec(void) {
    uintptr_t file_addr = initrd_get_file_addr("./prog.bin");

    if (file_addr == 0) {
        uart_puts("Could not find user program\n");
        return;
    }

    uint32_t data_size;
    uintptr_t file_data = initrd_get_filedata(file_addr, &data_size);

    char * proc_addr = allocate(4096);

    for (uintptr_t i = 0; i < data_size; i += 1) {
        proc_addr[i] = ((char *)file_data)[i];
    }

    interrupts_enter_umode((uintptr_t)proc_addr);
}
