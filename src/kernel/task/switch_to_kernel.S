.section ".text"
.globl switch_to_kernel
.p2align 2

switch_to_kernel:
  # --- [SAVE CURRENT CPU STATE] ---
  sd s0,  0(a0)
  sd s1,  8(a0)
  sd s2,  16(a0)
  sd s3,  24(a0)
  sd s4,  32(a0)
  sd s5,  40(a0)
  sd s6,  48(a0)
  sd s7,  56(a0)
  sd s8,  64(a0)
  sd s9,  72(a0)
  sd s10, 80(a0)
  sd s11, 88(a0)
  sd ra,  96(a0)
  sd sp,  104(a0)

  csrr t0, sscratch
  sd   t0, 112(a0)

  csrr t0, sstatus
  sd   t0, 120(a0)

  csrr t0, satp
  sd t0, 128(a0)

  # --- [RESTORE NEXT CPU STATE] ---
  ld s0, 0(a1)
  ld s1, 8(a1)
  ld s2, 16(a1)
  ld s3, 24(a1)
  ld s4, 32(a1)
  ld s5, 40(a1)
  ld s6, 48(a1)
  ld s7, 56(a1)
  ld s8, 64(a1)
  ld s9, 72(a1)
  ld s10, 80(a1)
  ld s11, 88(a1)
  ld ra, 96(a1)
  ld sp, 104(a1)

  mv tp, a1

  ld t0, 112(a1)
  csrw sscratch, t0

  ld   t0, 120(a1)
  csrw sstatus, t0

  ld t0, 128(a1)
  csrw satp, t0
  sfence.vma zero, zero

  # --- [EXECUTE NEXT THREAD] --
  ret
