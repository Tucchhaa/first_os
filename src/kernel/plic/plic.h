#pragma once

#include <stdint.h>

void plic_init(uintptr_t base);
void plic_enable_irq(uint32_t irq, uint32_t priority);
uint32_t plic_claim(void);
void plic_complete(uint32_t irq);
