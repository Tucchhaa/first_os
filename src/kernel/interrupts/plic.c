#include "plic.h"

#include "../../uart/uart_sync.h"
#include "../../fdt/fdt.h"
#include "../../string.h"
#include "../../converters.h"

static uint32_t _hart_ctx = 1;
static uintptr_t _plic_addr;

static inline volatile uint32_t * plic_priority_reg(uint32_t irq) {
    return (volatile uint32_t *)(_plic_addr + 0x0000 + irq * 4);
}

static inline volatile uint32_t * plic_enable_reg(uint32_t ctx, uint32_t irq) {
    return (volatile uint32_t *)(_plic_addr + 0x2000 + ctx * 0x80 + (irq / 32) * 4);
}

static inline volatile uint32_t * plic_threshold_reg(uint32_t ctx) {
    return (volatile uint32_t *)(_plic_addr + 0x200000 + ctx * 0x1000);
}

static inline volatile uint32_t * plic_claim_reg(uint32_t ctx) {
    return (volatile uint32_t *)(_plic_addr + 0x200004 + ctx * 0x1000);
}

void plic_setup() {
    uart_sync_puts("[KERNEL:PLIC] setting up...\n");

    uintptr_t plic_node = fdt_node_addr_by_compatible("riscv,plic0");

    if (plic_node == 0) {
        uart_sync_puts("[KERNEL:PLIC] no node compatible with 'riscv,plic0' under /soc\n");
        return;
    }

    fdt_reg_property(plic_node, &_plic_addr, (void *)0);
    *plic_threshold_reg(_hart_ctx) = 0;

    char buf[40];
    i64tox(_plic_addr, buf);
    uart_sync_puts_variadic("[KERNEL:PLIC] PLIC base: 0x", buf, "\n", 0);

    uart_sync_puts("[KERNEL:PLIC] Done setting up\n");
}

void plic_enable_irq(uint32_t irq, uint32_t priority) {
    *plic_priority_reg(irq) = priority;
    *plic_enable_reg(_hart_ctx, irq) |= (1u << (irq % 32));
}

uint32_t plic_claim(void) {
    return *plic_claim_reg(_hart_ctx);
}

void plic_complete(uint32_t irq) {
    *plic_claim_reg(_hart_ctx) = irq;
}
