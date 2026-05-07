.section ".text"
.globl _switch_to_user
.p2align 2

_switch_to_user:
  # --- [RESTORE CPU STATE] ---
  csrrw t0, sscratch, t0

  ld t1, 256(t0)
  csrw sepc, t1

  ld t1, 264(t0)
  csrw sstatus, t1
  
  ld x1, 8(t0)
  ld x2, 16(t0)
  ld x3, 24(t0)
  ld x4, 32(t0)
  # --- x5 reg is t0, skip for now ---
  ld x6, 48(t0)
  ld x7, 56(t0)
  ld x8, 64(t0)
  ld x9, 72(t0)
  ld x10, 80(t0)
  ld x11, 88(t0)
  ld x12, 96(t0)
  ld x13, 104(t0)
  ld x14, 112(t0)
  ld x15, 120(t0)
  ld x16, 128(t0)
  ld x17, 136(t0)
  ld x18, 144(t0)
  ld x19, 152(t0)
  ld x20, 160(t0)
  ld x21, 168(t0)
  ld x22, 176(t0)
  ld x23, 184(t0)
  ld x24, 192(t0)
  ld x25, 200(t0)
  ld x26, 208(t0)
  ld x27, 216(t0)
  ld x28, 224(t0)
  ld x29, 232(t0)
  ld x30, 240(t0)
  ld x31, 248(t0)

  csrw sscratch, t0

  # --- restore t0 ---
  ld t0, 40(t0)

  sret
