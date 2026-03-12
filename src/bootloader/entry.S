.section ".text"
.globl _start
.extern bootloader

_start:
  # --- zero bss ---
  la      t0, __bss_start
  la      t1, __bss_end
  bgeu    t0, t1, 2f

1:
  sd      zero, 0(t0)
  addi    t0, t0, 8
  bltu    t0, t1, 1b

2:
  # --- set up stack ---
  la sp, __stack_top

  # --- run bootloader ---
  call bootloader

3:
  # --- loop waiting for interrupts ---
  wfi
  j 3b
