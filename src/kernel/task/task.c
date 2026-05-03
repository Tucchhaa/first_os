#include "task.h"

void trapframe_init(struct trapframe * trapframe) {
    for (uint32_t i=0; i < 32; i++) {
        trapframe->regs[i] = 0;
    }

    trapframe->sepc = 0;
    trapframe->sstatus = 0;
}