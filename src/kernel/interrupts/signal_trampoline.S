.section ".text"
.globl _signal_trampoline
.p2align 2

_signal_trampoline:
  li a7, 11
  ecall
