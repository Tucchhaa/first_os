#include "string.h"

uint32_t kstrlen(const char * s) {
    int i = 0;

    while (s[i] != '\0') {
        i++;
    }

    return i;
}

uint8_t streqln(const char * a, const char * b, uint32_t len) {
    uint32_t i = 0;

    while (i < len && a[i] != '\0' && b[i] != '\0' && a[i] == b[i]) {
        i++;
    }

    return i == len || a[i] == b[i];
}

uint8_t streql(const char * a, const char * b) {
    return streqln(a, b, -1);
}

uint32_t strtoken(const char * s, const char * token, uint32_t offset) {
    uint32_t tokenlen = kstrlen(token);
    uint32_t i = offset;

    while (1) {
        if (s[i] == '\0' || streqln(&s[i], token, tokenlen)) {
            return i;
        }

        i++;
    }
}

void strslice(const char * s, char * buf, uint32_t offset, uint32_t n) {
    int i = 0;

    while (i < n) {
        buf[i] = s[offset + i];
        i++;
    } 

    buf[i] = '\0';
}

void _itox(int64_t x, char * const buf, int n) {
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
