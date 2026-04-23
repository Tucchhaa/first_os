#include "../uart.h"
#include "../sbi.h"
#include "../string.h"
#include "../fdt.h"
#include "../utils.h"
#include "mm/page_allocator.h"
#include "mm/dynamic_allocator.h"
#include "initrd.h"
#include "traps/trap.h"

extern char __kernel_start;
extern char __kernel_end;

uintptr_t fdt_addr;

struct fdt_node_cells fdt_cells;

uintptr_t initrd_start_addr;
uintptr_t initrd_end_addr;

static void setup_uart(void);
static void read_fdt(void);
static void setup_initrd(void);
static void setup_traps(void);
static void setup_memory(void);

static void command_info(void);
static void command_testfdt(void);
static void command_ls(void);
static void command_cat(const char * command);
static void command_testmm(void);
static void command_testmm1(void);
static void command_testmm2(void);
static void command_testmm3(void);
static void command_testmm4(void);
static void command_testmm5(void);
static void command_exec(void);

void kmain(uint64_t _hartid, uintptr_t _fdt_addr) {
    fdt_addr = _fdt_addr;

    if (fdt_check_magic(fdt_addr) == 0) {
        return;
    }

    setup_uart();
    read_fdt();
    setup_initrd();
    setup_memory();
    setup_traps();
    async_uart_setup();
    async_uart_puts("\n");
    
    const int command_max_size = 100;
    char command[command_max_size];

    while(1) {
        async_uart_puts("sh> ");
        async_uart_getline(command, command_max_size);

        if (streql(command, "help")) {
            async_uart_puts("Available commands:\n");
            async_uart_puts("  help - show all commands\n");
            async_uart_puts("  info - print system info\n");
            async_uart_puts("  ls - print file system.\n");
            async_uart_puts("  cat <filepath> - print contents of a file.\n");
            async_uart_puts("  testfdt - test fdt parser.\n");
            async_uart_puts("  setupmm - setup memory allocator.\n");
            async_uart_puts("  testmm - test memory allocator.\n");
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
        else if (streql(command, "setupmm")) {
            setup_memory();
        }
        else if (streql(command, "testmm1")) {
            command_testmm1();
        }
        else if (streql(command, "testmm2")) {
            command_testmm2();
        }
        else if (streql(command, "testmm3")) {
            command_testmm3();
        }
        else if (streql(command, "testmm4")) {
            command_testmm4();
        }
        else if (streql(command, "testmm5")) {
            command_testmm5();
        }
        else if (streql(command, "testmm")) {
            command_testmm();
        }
        else if (streql(command, "exec")) {
            command_exec();
        }
        else {
            async_uart_puts("Unknown command\n");
        }
    }
}

static void read_fdt(void) {
    uart_puts("[KERNEL] Reading FDT");

    uintptr_t root_node_addr = fdt_node_addr_by_path(fdt_addr, "/");
    fdt_cells = fdt_get_node_cells(fdt_addr, root_node_addr);

    if (fdt_cells.error) {
        uart_puts("[KERNEL:ERROR] Could not get soc node cells\n");

        // Fallback
        fdt_cells.address = 2;
        fdt_cells.size = 2;
    } else {
        uart_puts("[KERNEL] Reading FDT done");
    }
}

static void setup_uart(void) {
    uintptr_t soc_serial_node = fdt_node_addr_by_path(fdt_addr, "/soc/serial");
    struct fdt_property * serial_reg_prop = fdt_property_at_addr(
        fdt_property_addr_by_name(fdt_addr, soc_serial_node, "reg")
    );

    uintptr_t uart_base = fdt_cells.address == 1
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

        uintptr_t addr = initrd_start_addr;

        for(int i=0; i < 10; i++) {
            uint8_t a = *(uint8_t *)(addr + i);
            char b[10];
            i8tox(a, b);
            uart_puts(b);
            uart_puts(" ");
        }
        uart_puts("\n");
    }
}

static void setup_traps(void) {
    uintptr_t node_addr = fdt_node_addr_by_path(fdt_addr, "/cpus");
    uintptr_t prop_addr = fdt_property_addr_by_name(fdt_addr, node_addr, "timebase-frequency");
    
    if (prop_addr == 0) {
        uart_puts("/cpus[timebase-frequency] is not found\n");
        return;
    }

    struct fdt_property * prop = fdt_property_at_addr(prop_addr);
    uint32_t val = be32_to_cpu(*(uint32_t *)(&prop->data));
    
    trap_setup(val);

    schedule_interrupt();
}

static void setup_memory(void) {
    uart_puts("[KERNEL] Setting up memory\n");

    // TODO: support several memory nodes
    uintptr_t memory_node_addr = fdt_node_addr_by_path(fdt_addr, "/memory");
    uintptr_t device_type_prop_addr = fdt_property_addr_by_name(fdt_addr, memory_node_addr, "device_type");
    struct fdt_property * device_type_prop = fdt_property_at_addr(device_type_prop_addr);

    if (
        device_type_prop == 0
        || streql("memory", &device_type_prop->data) == 0
    ) {
        uart_puts("[KERNEL:ERROR] Unable to find memory node in FDT\n");
        return;
    }

    {
        uint64_t memory_base = 0, memory_size = 0;

        fdt_read_reg_property(
            fdt_addr, memory_node_addr, fdt_cells.address, fdt_cells.size,
            &memory_base, &memory_size
        );

        char buf1[32], buf2[32];
        i64tox(memory_base, buf1);
        i64tox(memory_size, buf2);
        uart_puts_variadic("[KERNEL] insert memory. base: 0x", buf1, ", size: 0x", buf2, "\n", 0);

        memory_add(memory_base, memory_size);
    }

    {
        uint64_t fdt_size = fdt_total_size(fdt_addr);
        uint64_t initrd_size = initrd_end_addr - initrd_start_addr;
        uint64_t kernel_size = (uint64_t)&__kernel_end - (uint64_t)&__kernel_start;

        memory_reserve(fdt_addr, fdt_size);
        memory_reserve(initrd_start_addr, initrd_size);
        memory_reserve((uint64_t)&__kernel_start, kernel_size);
    }

    {
        uintptr_t reserved_memory_node_addr = fdt_node_addr_by_path(fdt_addr, "/reserved-memory");

        if (reserved_memory_node_addr) {
            uintptr_t current_node = fdt_child_node(reserved_memory_node_addr);

            while (current_node != 0) {
                struct fdt_property * reg_prop = fdt_property_at_addr(
                    fdt_property_addr_by_name(fdt_addr, current_node, "reg")
                );

                uint64_t address, size;

                fdt_read_reg_property(
                    fdt_addr, current_node,
                    fdt_cells.address, fdt_cells.size,
                    &address, &size
                );

                memory_reserve(address, size);

                current_node = fdt_sibling_node(current_node);
            }
        }
    }

    if (memory_init()) {
        uart_puts("[KERNEL:ERROR] error occurred during memory init\n");
        return;
    }
    dynamic_allocator_init();

    uart_puts("[KERNEL] Done setting up memory\n");

    /*
    TODO:
    - support multiple memory regions
    - clean up the kmain.c
    - optimize page allocator
    - use single linked list
    - implement self relocating bootloader
    - memset to 0 allocated pages
    */
}

static void command_info(void) {
    char buf[20];

    async_uart_puts("System information:\n");

    if (sbi_get_spec_version().error) {
        async_uart_puts("  error occured\n");
        return;
    }

    i64tox(sbi_get_spec_version().value, buf);
    async_uart_puts_variadic("  OpenSBI specification version: 0x", buf, "\n", 0);

    i64tox(sbi_get_impl_id().value, buf);
    async_uart_puts_variadic("  implementation ID: 0x", buf, "\n", 0);

    i64tox(sbi_get_impl_version().value, buf);
    async_uart_puts_variadic("  implementation version: 0x", buf, "\n", 0);
    
    async_uart_puts("\n");
}

static void command_testfdt(void) {
    uintptr_t interrupt_controller_node = fdt_node_addr_by_path(
        fdt_addr, "/cpus/cpu@0/interrupt-controller"
    );
    struct fdt_property * compatible_prop = fdt_property_at_addr(
        fdt_property_addr_by_name(fdt_addr, interrupt_controller_node, "compatible")
    );

    if (compatible_prop == 0) {
        async_uart_puts("/cpus/cpu@0/interrupt-controller[compatible] - not found\n");
    } else {
        char buf[40];
        itoa(be32_to_cpu(compatible_prop->len), buf);
        async_uart_puts_variadic("property data len: ", buf, "\n\n", 0);

        async_uart_puts("compatible: ");

        uint32_t i = 0;

        while (i < be32_to_cpu(compatible_prop->len)) {
            async_uart_put(((const char *)&compatible_prop->data)[i]);
            i += 1;
        }
        async_uart_puts("\n");
    }

    // memory node
    uintptr_t memory_node = fdt_node_addr_by_path(
        fdt_addr, "/memory"
    );

    uint64_t memory_base = 0, memory_size = 0;
    fdt_read_reg_property(
        fdt_addr, memory_node, fdt_cells.address, fdt_cells.size,
        &memory_base, &memory_size
    );

    char buf1[32], buf2[32];
    i64tox(memory_base, buf1);
    i64tox(memory_size, buf2);

    async_uart_puts_variadic("memory: base=0x", buf1, " size=0x", buf2, "\n", 0);

    // initrd
    if (initrd_start_addr == 0) {
        async_uart_puts("/chosen[linux,initrd-start] - not found\n");
    } else {
        char buf1[40], buf2[40];
        i64tox(initrd_start_addr, buf1);
        i64tox(initrd_end_addr, buf2);
        async_uart_puts_variadic("initrd: start=0x", buf1, " end=0x", buf2, "\n", 0);
    }
}

static void command_ls(void) {
    if (initrd_check_magic(initrd_start_addr) == 0) {
        async_uart_puts("initrd was not found\n");
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
    async_uart_puts_variadic("Total ", buf, " files.\n", 0);

    // show all files
    file_addr = initrd_start_addr;

    while (file_addr != 0) {
        uint32_t path_size;
        const char * filepath = initrd_get_filepath(file_addr, &path_size);

        uint32_t file_data_size;
        initrd_get_filedata(file_addr, &file_data_size);

        itoa(file_data_size, buf);
        async_uart_puts(buf);

        int32_t space_count = 8 - kstrlen(buf);

        while (space_count) {
            async_uart_puts(" ");
            space_count -= 1;
        }

        for(int i=0; i < path_size; i++) {
            async_uart_put(filepath[i]);
        }
        async_uart_puts("\n");

        file_addr = initrd_get_next_file_addr(file_addr);
    }

    async_uart_puts("\n");
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
                
                if (c == '\n') { async_uart_put('\r'); }
                async_uart_put(c);
            }

            async_uart_puts("\n");
            return;
        }

        file_addr = initrd_get_next_file_addr(file_addr);
    }

    async_uart_puts("File not found\n\n");
}

static void command_testmm1() {
    async_uart_puts("Testing memory allocation...\n");
    char *ptr1 = (char *)allocate(4000);
    char *ptr2 = (char *)allocate(8000);
    char *ptr3 = (char *)allocate(4000);
    char *ptr4 = (char *)allocate(4000);

    free(ptr1);
    free(ptr2);
    free(ptr3);
    free(ptr4);
}

static void command_testmm2() {
    /* Test kmalloc */
    async_uart_puts("Testing dynamic allocator...\n");
    char *kmem_ptr1 = (char *)allocate(16);
    char *kmem_ptr2 = (char *)allocate(32);
    char *kmem_ptr3 = (char *)allocate(64);
    char *kmem_ptr4 = (char *)allocate(128);

    free(kmem_ptr1);
    free(kmem_ptr2);
    free(kmem_ptr3);
    free(kmem_ptr4);

    char *kmem_ptr5 = (char *)allocate(16);
    char *kmem_ptr6 = (char *)allocate(32);

    free(kmem_ptr5);
    free(kmem_ptr6);
}

static void command_testmm3() {
    // Test allocate new page if the cache is not enough
    void *kmem_ptr[102];
    for (int i=0; i<25; i++) {
        kmem_ptr[i] = (char *)allocate(512);
    }
    for (int i=0; i<25; i++) {
        free(kmem_ptr[i]);
    }
}

static void command_testmm4() {
    // Test exceeding the maximum size
    char *kmem_ptr7 = (char *)allocate(1 << 31);
    if (kmem_ptr7 == 0) {
        async_uart_puts("Allocation failed as expected for size > MAX_ALLOC_SIZE\n");
    }
    else {
        async_uart_puts("Unexpected allocation success for size > MAX_ALLOC_SIZE\n");
        free(kmem_ptr7);
    }
}

static void command_testmm5() {
    /***************** Case 1 *****************/

    async_uart_puts("\n===== Part 1 =====\n");

    void *p1 = allocate(4097);
    free(p1);

    async_uart_puts("\n=== Part 1 End ===\n");

    async_uart_puts("\n===== Part 2 =====\n");

    // Allocate all blocks at order 0, 1, 2 and 3
    int NUM_BLOCKS_AT_ORDER_0 = 0;  // Need modified
    int NUM_BLOCKS_AT_ORDER_1 = 0;
    int NUM_BLOCKS_AT_ORDER_2 = 0;
    int NUM_BLOCKS_AT_ORDER_3 = 0;

    void *ps0[NUM_BLOCKS_AT_ORDER_0];
    void *ps1[NUM_BLOCKS_AT_ORDER_1];
    void *ps2[NUM_BLOCKS_AT_ORDER_2];
    void *ps3[NUM_BLOCKS_AT_ORDER_3];
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_0; ++i) {
        ps0[i] = allocate(4096);
    }
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_1; ++i) {
        ps1[i] = allocate(8192);
    }
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_2; ++i) {
        ps2[i] = allocate(16384);
    }
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_3; ++i) {
        ps3[i] = allocate(32768);
    }

    async_uart_puts("\n-----------\n");

    long MAX_BLOCK_SIZE = PAGE_SIZE * (1 << 22);

    /* **DO NOT** uncomment this section */
    void *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10;

    p1 = allocate(4095);
    free(p1);                        // 4095
    p1 = allocate(4095);

    p2 = allocate(3769);
    p3 = allocate(2699);
    p4 = allocate(1028);
    p5 = allocate(1);
    p6 = allocate(4096);
    free(p5);                        // 1
    p7 = allocate(16000);
    free(p1);                        // 4095
    free(p4);                        // 1028
    free(p2);                        // 3769
    p8 = allocate(4097);
    p9 = allocate(MAX_BLOCK_SIZE + 1);
    p10 = allocate(MAX_BLOCK_SIZE);
    free(p6);                        // 4096
    free(p8);                        // 4097
    p2 = allocate(7197);

    free(p10);                       // MAX_BLOCK_SIZE
    free(p7);                        // 16000
    free(p2);                        // 7197
    free(p3);                        // 2699

    async_uart_puts("\n-----------\n");

    // Free all blocks remaining
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_0; ++i) {
        free(ps0[i]);
    }
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_1; ++i) {
        free(ps1[i]);
    }
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_2; ++i) {
        free(ps2[i]);
    }
    for (int i = 0; i < NUM_BLOCKS_AT_ORDER_3; ++i) {
        free(ps3[i]);
    }

    async_uart_puts("\n=== Part 2 End ===\n");
}

static void command_testmm() {
    command_testmm1();
    command_testmm2();
    command_testmm3();
    command_testmm4();
}

static void command_exec() {
    uintptr_t file_addr = initrd_get_file_addr(initrd_start_addr, "./prog.bin");

    if (file_addr == 0) {
        async_uart_puts("Could not find user program\n");
        return;
    }

    uint32_t data_size;
    uintptr_t file_data = initrd_get_filedata(file_addr, &data_size);

    char * proc_addr = allocate(4096);

    for (uintptr_t i = 0; i < data_size; i += 1) {
        proc_addr[i] = ((char *)file_data)[i];
    }

    uint64_t sstatus;
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    sstatus |= (1 << 5);     // Set SPIE

    asm volatile ("csrw sepc, %0" :: "r" (proc_addr) : "memory");
    asm volatile ("csrw sstatus, %0" :: "r" (sstatus) : "memory");
    asm volatile ("sret");
}