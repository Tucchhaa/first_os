#include "_uart_regs.h"

#include "../fdt/fdt_parser.h"

struct uart_regs uart_get_regs(uintptr_t serial_node) {
    uintptr_t uart_addr;
    fdt_reg_property(serial_node, &uart_addr, (void*)0);

    struct uart_regs result;
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
