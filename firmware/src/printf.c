/*
 * printf.c - Minimal printf implementation for bare-metal
 */

#include "arch/cc.h"
#include <stdarg.h>

/* Simple number to string conversion */
static int put_num(char *buf, size_t size, unsigned long val, int base, int is_signed, int width, char pad) {
    char tmp[24];
    int i = 0;
    int neg = 0;
    int written = 0;

    if (is_signed && (long)val < 0) {
        neg = 1;
        val = -(long)val;
    }

    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0) {
            int digit = val % base;
            tmp[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
            val /= base;
        }
    }

    /* Calculate padding */
    int numlen = i + neg;
    int padlen = (width > numlen) ? width - numlen : 0;

    /* Write padding */
    if (pad == '0' && neg) {
        if (written < (int)size - 1) buf[written] = '-';
        written++;
        neg = 0;
    }

    while (padlen-- > 0) {
        if (written < (int)size - 1) buf[written] = pad;
        written++;
    }

    /* Write sign */
    if (neg) {
        if (written < (int)size - 1) buf[written] = '-';
        written++;
    }

    /* Write digits */
    while (i-- > 0) {
        if (written < (int)size - 1) buf[written] = tmp[i];
        written++;
    }

    return written;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    int written = 0;

    if (size == 0) return 0;

    while (*fmt && written < (int)size - 1) {
        if (*fmt != '%') {
            buf[written++] = *fmt++;
            continue;
        }

        fmt++;  /* Skip '%' */

        /* Parse width */
        int width = 0;
        char pad = ' ';
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Parse length modifier */
        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
            if (*fmt == 'l') {
                fmt++;  /* Skip second 'l' */
            }
        }

        /* Parse conversion specifier */
        switch (*fmt) {
            case 'd':
            case 'i': {
                long val = is_long ? va_arg(ap, long) : va_arg(ap, int);
                written += put_num(buf + written, size - written, val, 10, 1, width, pad);
                break;
            }
            case 'u': {
                unsigned long val = is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
                written += put_num(buf + written, size - written, val, 10, 0, width, pad);
                break;
            }
            case 'x':
            case 'X': {
                unsigned long val = is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
                written += put_num(buf + written, size - written, val, 16, 0, width, pad);
                break;
            }
            case 'p': {
                unsigned long val = (unsigned long)va_arg(ap, void *);
                if (written < (int)size - 1) buf[written++] = '0';
                if (written < (int)size - 1) buf[written++] = 'x';
                written += put_num(buf + written, size - written, val, 16, 0, 0, ' ');
                break;
            }
            case 'c': {
                int c = va_arg(ap, int);
                if (written < (int)size - 1) buf[written++] = c;
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s && written < (int)size - 1) {
                    buf[written++] = *s++;
                }
                break;
            }
            case '%':
                if (written < (int)size - 1) buf[written++] = '%';
                break;
            default:
                /* Unknown format - just copy */
                if (written < (int)size - 1) buf[written++] = '%';
                if (written < (int)size - 1) buf[written++] = *fmt;
                break;
        }
        fmt++;
    }

    buf[written] = '\0';
    return written;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}
