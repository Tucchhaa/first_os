#include "setup.h"

#include <stdint.h>

#include "../../uart.h"
#include "../../fdt/fdt.h"
#include "../../string.h"
#include "../initrd/initrd.h"
#include "page_allocator.h"
#include "dynamic_allocator.h"

extern char __kernel_start;
extern char __kernel_end;

void memory_setup() {
    uart_puts("[KERNEL] Setting up memory\n");

    // TODO: support several memory nodes
    uintptr_t memory_node_addr = fdt_node_addr_by_path("/memory");
    struct fdt_property * device_type_prop = fdt_property_by_name(memory_node_addr, "device_type");

    if (
        device_type_prop == 0 || streql("memory", &device_type_prop->data) == 0
    ) {
        uart_puts("[KERNEL:ERROR] Unable to find memory node in FDT\n");
        return;
    }

    {
        uint64_t memory_base = 0, memory_size = 0;

        fdt_read_reg_property(memory_node_addr, &memory_base, &memory_size);

        char buf1[32], buf2[32];
        i64tox(memory_base, buf1);
        i64tox(memory_size, buf2);
        uart_puts_variadic("[KERNEL] insert memory. base: 0x", buf1, ", size: 0x", buf2, "\n", 0);

        memory_add(memory_base, memory_size);
    }

    {
        uint64_t fdt_size = fdt_total_size();
        uint64_t initrd_size = initrd_end_addr - initrd_start_addr;
        uint64_t kernel_size = (uint64_t)&__kernel_end - (uint64_t)&__kernel_start;

        char buf1[32], buf2[32];

        i64tox(fdt_addr, buf1);
        i64tox(fdt_size, buf2);
        uart_puts_variadic("[KERNEL] reserve FDT memory. base: 0x", buf1, ", size: 0x", buf2, "\n", 0);
        memory_reserve(fdt_addr, fdt_size);


        i64tox(initrd_start_addr, buf1);
        i64tox(initrd_size, buf2);
        uart_puts_variadic("[KERNEL] reserve INITRD memory. base: 0x", buf1, ", size: 0x", buf2, "\n", 0);
        memory_reserve(initrd_start_addr, initrd_size);

        i64tox(__kernel_start, buf1);
        i64tox(kernel_size, buf2);
        uart_puts_variadic("[KERNEL] reserve Kernel memory. base: 0x", buf1, ", size: 0x", buf2, "\n", 0);
        memory_reserve((uint64_t)&__kernel_start, kernel_size);
    }

    {
        uintptr_t reserved_memory_node_addr = fdt_node_addr_by_path("/reserved-memory");

        if (reserved_memory_node_addr) {
            uintptr_t current_node = fdt_child_node(reserved_memory_node_addr);

            while (current_node != 0) {
                uint64_t address, size;
                fdt_read_reg_property(current_node, &address, &size);

                char buf1[32], buf2[32];
                i64tox(address, buf1);
                i64tox(size, buf2);
                uart_puts_variadic("[KERNEL] reserve memory. base: 0x", buf1, ", size: 0x", buf2, "\n", 0);

                memory_reserve(address, size);

                current_node = fdt_sibling_node(current_node);
            }
        }
    }

    if (memory_init() == 0) {
        uart_puts("[KERNEL:ERROR] error occurred during memory init\n");
        return;
    }
    dynamic_allocator_init();

    uart_puts("[KERNEL] Done setting up memory\n");
}
