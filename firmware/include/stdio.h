/*
 * stdio.h - Minimal stub for bare-metal
 */
#ifndef _STDIO_H
#define _STDIO_H

#include "arch/cc.h"

/* File type for printf compatibility */
typedef void FILE;

/* Standard streams - stub */
#define stdout ((FILE*)1)
#define stderr ((FILE*)2)

/* lwIP debug uses snprintf */
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, ...);

/* Used by debug macros */
#define printf(...) console_printf(__VA_ARGS__)
extern void console_printf(const char *fmt, ...);

/* fflush - no-op for bare-metal */
static inline int fflush(FILE *stream) {
    (void)stream;
    return 0;
}

#endif /* _STDIO_H */
