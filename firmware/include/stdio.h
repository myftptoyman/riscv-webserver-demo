/*
 * stdio.h - Minimal stub for bare-metal
 */
#ifndef _STDIO_H
#define _STDIO_H

#include "arch/cc.h"

/* lwIP debug uses snprintf */
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, ...);

/* Used by debug macros */
#define printf(...) console_printf(__VA_ARGS__)
extern void console_printf(const char *fmt, ...);

#endif /* _STDIO_H */
