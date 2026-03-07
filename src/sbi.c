#include "sbi.h"

struct sbiret sbi_ecall(
    int ext, int fid,
    unsigned long arg0,
    unsigned long arg1,
    unsigned long arg2,
    unsigned long arg3,
    unsigned long arg4,
    unsigned long arg5
) {
    register long a0 asm("a0") = arg0;
    register long a1 asm("a1") = arg1;
    register long a2 asm("a2") = arg2;
    register long a3 asm("a3") = arg3;
    register long a4 asm("a4") = arg4;
    register long a5 asm("a5") = arg5;
    register long a6 asm("a6") = fid;
    register long a7 asm("a7") = ext;

    asm volatile (
        "ecall"
        : "+r"(a0), "+r"(a1)
        : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
        : "memory"
    );

    struct sbiret ret;
    ret.error = a0;
    ret.value = a1;

    return ret;
}

struct sbiret sbi_ecall_default(int ext, int fid) {
    return sbi_ecall(ext, fid, 0, 0, 0, 0, 0, 0);
}


struct sbiret sbi_get_spec_version(void) {
    return sbi_ecall_default(0x10, 0x0);
}

struct sbiret sbi_get_impl_id(void) {
    return sbi_ecall_default(0x10, 0x1);
}

struct sbiret sbi_get_impl_version(void) {
    return sbi_ecall_default(0x10, 0x2);
}
