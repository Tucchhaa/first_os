.section ".text"
.globl _trap_entry
.p2align 2

_trap_entry:
  # --- [SAVE CPU STATE] ---

  # --- sscratch is a pointer to cpu frame  ---
  csrrw t0, sscratch, t0

  sd x1, 8(t0)
  sd x2, 16(t0)
  sd x3, 24(t0)
  sd x4, 32(t0)
  # --- x5 reg is t0, skip for now ---
  sd x6, 48(t0)
  sd x7, 56(t0)
  sd x8, 64(t0)
  sd x9, 72(t0)
  sd x10, 80(t0)
  sd x11, 88(t0)
  sd x12, 96(t0)
  sd x13, 104(t0)
  sd x14, 112(t0)
  sd x15, 120(t0)
  sd x16, 128(t0)
  sd x17, 136(t0)
  sd x18, 144(t0)
  sd x19, 152(t0)
  sd x20, 160(t0)
  sd x21, 168(t0)
  sd x22, 176(t0)
  sd x23, 184(t0)
  sd x24, 192(t0)
  sd x25, 200(t0)
  sd x26, 208(t0)
  sd x27, 216(t0)
  sd x28, 224(t0)
  sd x29, 232(t0)
  sd x30, 240(t0)
  sd x31, 248(t0)

  # --- save t0 ---
  csrr t1, sscratch
  sd t1, 40(t0)

  csrr t1, sepc
  sd t1, 256(t0)

  csrr t1, sstatus
  sd t1, 264(t0)

  # --- finish saving cpu state ---
  csrw sscratch, t0

  # --- [JUMP TO TRAP HANDLER] ---
  call trap_handler

  # --- [RESTORE CPU STATE] ---

  # --- update sepc ---
  csrr t0, sscratch

  ld t1, 256(t0)
  csrw sepc, t1

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

  # --- restore t0 ---
  ld t0, 40(t0)

  # --- return control ---
  sret
