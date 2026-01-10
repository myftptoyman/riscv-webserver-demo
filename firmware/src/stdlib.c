/*
 * stdlib.c - Minimal stdlib functions for bare-metal
 */

#include "arch/cc.h"

int atoi(const char *s) {
    int n = 0;
    int neg = 0;

    while (*s == ' ' || *s == '\t' || *s == '\n') s++;

    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }

    return neg ? -n : n;
}

long atol(const char *s) {
    return (long)atoi(s);
}

long strtol(const char *s, char **endptr, int base) {
    long n = 0;
    int neg = 0;
    const char *start = s;

    while (*s == ' ' || *s == '\t' || *s == '\n') s++;

    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    /* Auto-detect base */
    if (base == 0) {
        if (*s == '0') {
            s++;
            if (*s == 'x' || *s == 'X') {
                base = 16;
                s++;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && *s == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (*s >= 'a' && *s <= 'z') {
            digit = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'Z') {
            digit = *s - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) break;

        n = n * base + digit;
        s++;
    }

    if (endptr) {
        *endptr = (s == start) ? (char *)start : (char *)s;
    }

    return neg ? -n : n;
}

unsigned long strtoul(const char *s, char **endptr, int base) {
    return (unsigned long)strtol(s, endptr, base);
}

void abort(void) {
    /* HTIF exit with error code */
    extern volatile uint64_t tohost;
    extern volatile uint64_t fromhost;
    while (tohost) {
        fromhost = 0;
    }
    tohost = (1 << 1) | 1;  /* Exit code 1 */
    while (1);
}

/* Memory allocation wrappers for lwext4 */
extern void *malloc(size_t size);
extern void free(void *ptr);
extern void *calloc(size_t nmemb, size_t size);

void *ext4_user_malloc(size_t size) {
    return malloc(size);
}

void *ext4_user_calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

void ext4_user_free(void *ptr) {
    free(ptr);
}

/* Simple qsort implementation (shell sort) */
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {
    unsigned char *arr = base;
    unsigned char *temp;
    size_t gap, i, j;

    if (nmemb <= 1) return;

    /* Allocate temp buffer on stack for small elements, otherwise on heap */
    unsigned char stack_buf[64];
    if (size <= sizeof(stack_buf)) {
        temp = stack_buf;
    } else {
        temp = malloc(size);
        if (!temp) return;
    }

    /* Shell sort with Ciura gap sequence */
    static const size_t gaps[] = {701, 301, 132, 57, 23, 10, 4, 1};
    for (int g = 0; g < 8; g++) {
        gap = gaps[g];
        if (gap >= nmemb) continue;

        for (i = gap; i < nmemb; i++) {
            __builtin_memcpy(temp, arr + i * size, size);
            j = i;
            while (j >= gap && compar(arr + (j - gap) * size, temp) > 0) {
                __builtin_memcpy(arr + j * size, arr + (j - gap) * size, size);
                j -= gap;
            }
            __builtin_memcpy(arr + j * size, temp, size);
        }
    }

    if (size > sizeof(stack_buf)) {
        free(temp);
    }
}
