#include "plic.h"

/*
PLIC register layout (sifive,plic-1.0.0):
  priority[irq]              : _plic_base + 0x0000 + irq * 4
  pending bits               : _plic_base + 0x1000 + (irq / 32) * 4
  enable bits[context]       : _plic_base + 0x2000 + context * 0x80 + (irq / 32) * 4
  threshold[context]         : _plic_base + 0x200000 + context * 0x1000
  claim/complete[context]    : _plic_base + 0x200004 + context * 0x1000
*/

#define PLIC_HART_CONTEXT 1

static uintptr_t _plic_base;

#define PLIC_PRIORITY(irq)       ((volatile uint32_t *)(_plic_base + 0x0000 + (irq) * 4))
#define PLIC_ENABLE(ctx, irq)    ((volatile uint32_t *)(_plic_base + 0x2000 + (ctx) * 0x80 + ((irq) / 32) * 4))
#define PLIC_THRESHOLD(ctx)      ((volatile uint32_t *)(_plic_base + 0x200000 + (ctx) * 0x1000))
#define PLIC_CLAIM(ctx)          ((volatile uint32_t *)(_plic_base + 0x200004 + (ctx) * 0x1000))

void plic_init(uintptr_t base) {
    _plic_base = base;
    *PLIC_THRESHOLD(PLIC_HART_CONTEXT) = 0;
}

void plic_enable_irq(uint32_t irq, uint32_t priority) {
    *PLIC_PRIORITY(irq) = priority;
    *PLIC_ENABLE(PLIC_HART_CONTEXT, irq) |= (1u << (irq % 32));
}

uint32_t plic_claim(void) {
    return *PLIC_CLAIM(PLIC_HART_CONTEXT);
}

void plic_complete(uint32_t irq) {
    *PLIC_CLAIM(PLIC_HART_CONTEXT) = irq;
}
