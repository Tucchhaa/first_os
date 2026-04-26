/*
sstatus
sstatus.SPP - stores CPU-mode before the trap
sstatus.SIE - whether interrupts are enabled
sstatus.SPIE - Saves the old SIE value when a trap occurs

stvec - points to trap handler. On any trap CPU jumps here.

sepc - on a trap CPU saved PC of the interrupts instruction,
it can be used to return control to the application

scause - top bit indicates if trap happened by interrupt or exception

stval - extra context about a trap

sscractch - kernel can use however it wants

sie - mask for interrupts
STIE - timer interrupts enabled
*/

#pragma once

#include <stdint.h>

typedef uint32_t csr_sie_flag;
const csr_sie_flag CSR_SIE_STIE = 1 << 5; // timer interrupts

typedef uint32_t csr_sstatus_flag;
const csr_sstatus_flag CSR_SSTATUS_SIE = 1 << 1; // interrupts enabled
const csr_sstatus_flag CSR_SSTATUS_SPIE = 1 << 5; // previous interrupts enabled
const csr_sstatus_flag CSR_SSTATUS_SPP = 1 << 8; // CPU-mode before trap

static inline void csr_sstatus_enable(csr_sstatus_flag flag) {
    asm volatile("csrs sstatus, %0" :: "r"(flag) : "memory");
}

static inline void csr_sstatus_disable(csr_sstatus_flag flag) {
    asm volatile("csrc sstatus, %0" :: "r"(flag) : "memory");
}

static inline void csr_sstatus_set(csr_sstatus_flag flag) {
    asm volatile("csrw sstatus, %0" :: "r"(flag) : "memory");
}

static inline void csr_sie_enable(csr_sie_flag flag) {
    asm volatile("csrs sie, %0" :: "r"(flag));
}

static inline void csr_sie_disable(csr_sie_flag flag) {
    asm volatile("csrc sie, %0" :: "r"(flag));
}

static inline void csr_stvec_set(uintptr_t handler_addr) {
    // last two bits used for mode: 0 - direct, 1 - vectored
    uintptr_t value = (uintptr_t)handler_addr & ~3;

    asm volatile ("csrw stvec, %0" :: "r"(value) : "memory");
}

static inline void csr_sscratch_set(uintptr_t value) {
    asm volatile ("csrw sscratch, %0" :: "r"(value) : "memory");
}

static inline void csr_sepc_set(uintptr_t value) {
    asm volatile ("csrw sepc, %0" :: "r"(value) : "memory");
}

static inline uint64_t csr_scause_get() {
    uint64_t value;
    asm volatile ("csrr %0, scause" : "=r" (value));
    return value;
}

static inline void csr_sret() {
    asm volatile ("sret");
}