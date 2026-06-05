#include "../uart/uart_sync.h"
#include "../uart/uart.h"
#include "../string.h"
#include "../fdt/fdt.h"
#include "../converters.h"
#include "sbi.h"
#include "interrupts/plic.h"
#include "interrupts/interrupt_handler.h"
#include "interrupts/interrupt_control.h"
#include "interrupts/timeouts.h"
#include "initrd/initrd.h"
#include "mm/setup.h"
#include "mm/page_allocator.h"
#include "mm/dynamic_allocator.h"
#include "task/task.h"
#include "task/kthreads.h"
#include "task/cpu_scheduler.h"
#include "../drivers/video/video.h"
#include "vmm/virtual_memory_setup.h"
#include "vmm/virtual_memory.h"

static void kernel_setup(uint64_t _fdt_addr);

static void kernel_cli();

static void schedule_timeout(void *);

void thread_entry() {
    char a[40], b[40];
    itoa(get_current_task()->pid, a);

    for (int i = 0; i < 5; i++) {
        itoa(i, b);

        uart_puts_variadic("Thread id: ", a, " ", b, "\n", 0);

        for (int j = 0; j < 100000000; j++);

        uart_puts("next\n");
        cpu_scheduler_next();
    }

    cpu_scheduler_kill();
}

void aboba (void *) {
    uart_puts("kernel is live\n");
    set_timeout(aboba, 0, 3000000);
}

/*
TODO:
- support multiple memory regions
- clean up the kmain.c
- optimize page allocator
- implement self relocating bootloader
- support interrupt task preemption
- rename setup func to init
- timer is called after a symbol is typed to the user program. Why?
- Free program code memory after task has completed (memleak)
- move uart and plic to drivers directory
- bug on opirv2, after a process is killed, 'exec' command doesn't work 
- since vm is used, the same linker script can be used for kernel
- implement print()
*/
void kmain(
    uint64_t _hartid, 
    uintptr_t _fdt_addr, 
    uint64_t _virtual_kernel_offset
) {
    set_virtual_kernel_offset(_virtual_kernel_offset);
    kernel_setup(_fdt_addr);

    struct task * cli_task = kthread_create(kernel_cli);
    cpu_scheduler_add_task(cli_task);

    // for (int i=0; i < 3; i++) {
    //     struct task * task = kthread_create(thread_entry);
    //     cpu_scheduler_add_task(task);
    // }

    aboba(0);

    cpu_scheduler_idle();
}

static void kernel_setup(uint64_t _fdt_addr) {
    if (fdt_setup(pa2va(_fdt_addr))) {
        return;
    }

    // Note: setup UART at physical addr for debugging
    uart_sync_setup();

    initrd_setup();
    memory_setup();

    // virtual_memory_drop_identity_mapping();
    virtual_memory_refine_mappings();
    uart_sync_setup();

    sbi_setup();
    video_setup();
    interrupt_setup();
    plic_setup();
    uart_setup();

    interrupts_enable_external();
    interrupts_enable_timer();

    cpu_scheduler_init();
}

static void command_info(void);
static void command_ls(void);
static void command_cat(const char * command);
static void command_exec(void);
static void command_settimeout(const char * command);

static void kernel_cli() {
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
            uart_puts("  settimeout <seconds> <message> - prints message after seconds\n");
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
        else if (streqln(command, "settimeout ", 11)) {
            command_settimeout(command);
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

        uint32_t data_size;
        initrd_get_filedata(file_addr, 0, &data_size);

        itoa(data_size, buf);
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

    uintptr_t file_addr = initrd_start_addr;

    while (file_addr != 0) {
        uint32_t path_size;
        const char * filepath = initrd_get_filepath(file_addr, &path_size);

        if (streqln(file_name, filepath, path_size)) {
            uintptr_t data;
            uint32_t data_size;
            initrd_get_filedata(file_addr, &data, &data_size);

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
    struct task * task = task_create_user("./osctest.bin");

    if (task == 0) {
        uart_puts("Could\'t create task\n");
        return;
    }

    kthread_exec_user(task);

    union task_wait_event_arg arg = { .i = task->pid };
    cpu_scheduler_wait_arg(TASK_WAIT_PROCESS_KILL, arg);
}

static void timeout_func(void * arg) {
    char * message = (char *)arg;

    uart_puts(message);
    uart_puts("\n");

    free(arg);
}

static void command_settimeout(const char * command) {
    char seconds_buf[20];
    uint32_t seconds_offset = 11;
    uint32_t seconds_len = 0;

    while (is_numeric(command[seconds_offset + seconds_len])) {
        seconds_buf[seconds_len] = command[seconds_offset + seconds_len];
        seconds_len += 1;
    }
    seconds_buf[seconds_len] = '\0';

    uint32_t seconds = atoi(seconds_buf);

    char * message = allocate(100);
    uint32_t message_offset = seconds_offset + seconds_len;
    uint32_t message_len = 0;

    while (command[message_offset + message_len] != '\0') {
        message[message_len] = command[message_offset + message_len];
        message_len += 1;
    }
    message[message_len] = '\0';

    if (seconds_len == 0 || message_len == 0) {
        uart_puts("Wrong parameters\n");
        free(message);
        return;
    }

    set_timeout(timeout_func, message, seconds * 1000000);
}

static void schedule_timeout(void *) {
    char c[40];
    itoa((sbi_read_time() / cpu_frequency), c);
    uart_puts_variadic("boot time: ", c , "\n", 0);

    set_timeout(schedule_timeout, (void *)0, 10000000);
}
