#include "interrupt_control.h"

#include "csr.h"

void interrupts_enable() { csr_sstatus_enable(CSR_SSTATUS_SIE); }
void interrupts_restore(uint8_t pie) { if (pie) csr_sstatus_enable(CSR_SSTATUS_SIE); }
uint8_t interrupts_disable() { return csr_sstatus_rdisable(CSR_SSTATUS_SIE); }

void interrupts_enable_external() { csr_sie_enable(CSR_SIE_SEIE); }
void interrupts_disable_external() { csr_sie_disable(CSR_SIE_SEIE); }

void interrupts_enable_timer() { csr_sie_enable(CSR_SIE_STIE); }
void interrupts_disable_timer() { csr_sie_disable(CSR_SIE_STIE); }
