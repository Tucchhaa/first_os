#include "process.h"

#include "../mm/dynamic_allocator.h"
#include "../interrupts/csr.h"

struct process * process_create(uintptr_t entry_addr, uint8_t is_umode) {
    struct process * process = allocate(sizeof(struct process));

    for (uint32_t i = 0; i < 32; i += 1) {
        process->regs[i] = 0;
    }

    process->sepc = entry_addr;
    process->sstatus = CSR_SSTATUS_SPIE;
    
    if (is_umode == 0) {
        process->sstatus |= CSR_SSTATUS_SPP;
    }

    return process;
}

void process_switch(struct process * next) {
    csr_sstatus_set_flag(CSR_SSTATUS_SPIE, next->sstatus & CSR_SSTATUS_SPIE);
    csr_sstatus_set_flag(CSR_SSTATUS_SPP, next->sstatus & CSR_SSTATUS_SPP);

    csr_sepc_set(next->sepc);
    csr_sscratch_set((uintptr_t)next);

    csr_sret();
}
