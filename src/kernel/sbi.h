#pragma once

#include <stdint.h>

#define SBI_TIME_EXTENSION 0x54494D45
#define SBI_LEGACY_TIME_EXTENSION 0x00

struct sbiret {
    long error;
    long value;
};

struct sbiret sbi_ecall(
    int ext, int fid,
    unsigned long arg0,
    unsigned long arg1,
    unsigned long arg2,
    unsigned long arg3,
    unsigned long arg4,
    unsigned long arg5
);
struct sbiret sbi_ecall_default(int ext, int fid);

struct sbiret sbi_probe_extension(int ext);
struct sbiret sbi_get_spec_version(void);
struct sbiret sbi_get_impl_id(void);
struct sbiret sbi_get_impl_version(void);

struct sbiret sbi_set_timer(uint64_t target_time);

uint64_t sbi_read_time();

void sbi_setup();
