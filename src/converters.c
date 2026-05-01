#include "converters.h"

uint32_t be32_to_cpu(uint32_t x) {
    return ((x & 0xFF000000) >> 24) |
           ((x & 0x00FF0000) >>  8) |
           ((x & 0x0000FF00) <<  8) |
           ((x & 0x000000FF) << 24);
}

uint64_t be64_to_cpu(uint64_t x) {
    return ((x & 0xFF00000000000000ULL) >> 56) |
           ((x & 0x00FF000000000000ULL) >> 40) |
           ((x & 0x0000FF0000000000ULL) >> 24) |
           ((x & 0x000000FF00000000ULL) >>  8) |
           ((x & 0x00000000FF000000ULL) <<  8) |
           ((x & 0x0000000000FF0000ULL) << 24) |
           ((x & 0x000000000000FF00ULL) << 40) |
           ((x & 0x00000000000000FFULL) << 56);
}

static void _itox(int64_t x, char * const buf, int n) {
    const char hex[] = "0123456789abcdef";

    for(int i = 0; i < n; i++) {
        buf[n - 1 - i] = hex[x & 0xF];
        x >>= 4;
    }

    buf[n] = '\0';
}

// buf must have size 17
void i64tox(int64_t x, char * const buf) { return _itox(x, buf, 16); }

// buf must have size 9
void i32tox(int32_t x, char * const buf) { return _itox(x, buf, 8); }

// buf must have size 3
void i8tox(int8_t x, char * const buf) { return _itox(x, buf, 2); }

// buf must have size 8
uint32_t xtoi32(char * const buf) {
    uint32_t result = 0;
    uint32_t i = 0;

    while (buf[i] != '\0' && i < sizeof(uint32_t) * 2) {
        char c = buf[i];
        uint8_t digit;

        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = 10 + (c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            digit = 10 + (c - 'A');
        } else {
            break;
        }

        result = (result << 4) | digit;
        i++;
    }

    return result;
}

// buf size must be at least 12
void itoa(int64_t x, char * const buf) {
    if (x == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    int64_t value = x;
    uint8_t is_negative = 0;

    if (value < 0) {
        is_negative = 1;
        value = -value;
    }

    char digits[10];
    uint32_t n = 0;

    while (value > 0) {
        digits[n++] = '0' + (value % 10);
        value /= 10;
    }

    uint32_t i = 0;

    if (is_negative) {
        buf[i++] = '-';
    }

    while (n > 0) {
        buf[i++] = digits[--n];
    }

    buf[i] = '\0';
}

uint64_t atoi(char * const buf) {
    uint64_t result = 0;
    uint32_t i = 0;

    while (buf[i] != '\0') {
        result = result * 10 + (buf[i] - '0');
        i++;
    }

    return result;
}
