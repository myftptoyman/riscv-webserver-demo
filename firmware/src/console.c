#include "console.h"
#include "platform.h"
#include <stdarg.h>

/* HTIF interface for output (works in bare-metal Spike) */
volatile uint64_t tohost __attribute__((section(".htif")));
volatile uint64_t fromhost __attribute__((section(".htif")));

void console_init(void) {
    /* No initialization needed for HTIF */
}

void console_putc(char c) {
    /* Wait for previous command to complete */
    while (tohost) {
        fromhost = 0;
    }
    /* Send character via HTIF console */
    tohost = ((uint64_t)1 << 56) | ((uint64_t)1 << 48) | (unsigned char)c;
    /* Wait for completion */
    while (tohost) {
        fromhost = 0;
    }
}

void console_puts(const char *s) {
    while (*s) {
        console_putc(*s++);
    }
}

static void print_unsigned(unsigned long val, int base) {
    char buf[20];
    int i = 0;

    if (val == 0) {
        console_putc('0');
        return;
    }

    while (val > 0) {
        int digit = val % base;
        buf[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        val /= base;
    }

    while (i > 0) {
        console_putc(buf[--i]);
    }
}

static void print_signed(long val) {
    if (val < 0) {
        console_putc('-');
        val = -val;
    }
    print_unsigned((unsigned long)val, 10);
}

void console_print_hex(unsigned long val) {
    console_puts("0x");
    print_unsigned(val, 16);
}

void console_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            console_putc(*fmt++);
            continue;
        }

        fmt++; /* skip '%' */

        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char*);
            console_puts(s ? s : "(null)");
            break;
        }
        case 'd': {
            int val = va_arg(ap, int);
            print_signed(val);
            break;
        }
        case 'u': {
            unsigned int val = va_arg(ap, unsigned int);
            print_unsigned(val, 10);
            break;
        }
        case 'x': {
            unsigned int val = va_arg(ap, unsigned int);
            print_unsigned(val, 16);
            break;
        }
        case 'l': {
            fmt++;
            if (*fmt == 'x') {
                unsigned long val = va_arg(ap, unsigned long);
                print_unsigned(val, 16);
            } else if (*fmt == 'u') {
                unsigned long val = va_arg(ap, unsigned long);
                print_unsigned(val, 10);
            } else if (*fmt == 'd') {
                long val = va_arg(ap, long);
                print_signed(val);
            }
            break;
        }
        case 'p': {
            unsigned long val = va_arg(ap, unsigned long);
            console_puts("0x");
            print_unsigned(val, 16);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            console_putc(c);
            break;
        }
        case '%':
            console_putc('%');
            break;
        default:
            console_putc('%');
            console_putc(*fmt);
            break;
        }
        fmt++;
    }

    va_end(ap);
}
