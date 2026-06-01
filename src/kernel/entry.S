.section ".text.init"
.globl _start
.extern kmain
.extern virtual_memory_setup

_start:
  # --- init bss ---
  lla      t0, __bss_start
  lla      t1, __bss_end
  bgeu    t0, t1, 2f

1:
  sd      zero, 0(t0)
  addi    t0, t0, 8
  bltu    t0, t1, 1b

2:
  # --- init stack ---
  lla sp, __stack_top

  # --- init virtual memory ---
  mv s0, a0 # hardid
  mv s1, a1 # fdt_addr

  mv a0, a1

  call virtual_memory_setup

  li t0, 1
  beq a0, t0, 3f

  # --- jump to high addresses ---
  lla t0, .Llink_addr
  ld t0, 0(t0)
  lla t1, __kernel_start
  sub t2, t0, t1

  lla t0, .Lhigh_entry
  add t0, t0, t2

  jr t0

3:
  # --- loop waiting for interrupts ---
  wfi
  j 3b

.Lhigh_entry:
  add sp, sp, t2
  mv a0, s0
  mv a1, s1
  mv a2, t2
  call kmain
  j 3b

.section .rodata
.align 3
.Llink_addr:
  .dword __kernel_start