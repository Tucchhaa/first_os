#include "_uart_regs.h"

#include "../fdt/fdt.h"

#if defined(KERNEL)
#include "../kernel/vmm/virtual_memory.h"
#endif

struct uart_regs uart_get_regs(uintptr_t serial_node) {
    uintptr_t uart_addr, uart_size;
    fdt_reg_property(serial_node, &uart_addr, &uart_size);

    struct uart_regs result;

    #if defined(KERNEL)
    if (virtual_memory_ready) {
        uart_addr = virtual_memory_map_mmio(uart_addr, uart_size);
    }
    #endif

    result.base = (uint8_t *)uart_addr;

    if (fdt_node_is_compatible(serial_node, "ns16550a")) {
        result.status = (uint8_t *)(uart_addr + 0x05);
        result.ier = (uint8_t *)(uart_addr + 0x01);
        result.mcr = (uint8_t *)(uart_addr + 0x04);
    } 
    else if (fdt_node_is_compatible(serial_node, "ky,pxa-uart")) {
        result.status = (uint8_t *)(uart_addr + 0x14);
        result.ier = (uint8_t *)(uart_addr + 0x4);
        result.mcr = (uint8_t *)(uart_addr + 0x10);
    }

    return result;
}
