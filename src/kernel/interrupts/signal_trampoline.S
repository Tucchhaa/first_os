.section ".text"
.globl _signal_trampoline
.p2align 12
_signal_trampoline:
  li a7, 11
  ecall
.p2align 12