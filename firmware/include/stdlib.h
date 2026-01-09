/*
 * stdlib.h - Minimal stub for bare-metal
 */
#ifndef _STDLIB_H
#define _STDLIB_H

#include "arch/cc.h"

/* Memory allocation */
extern void *malloc(size_t size);
extern void free(void *ptr);
extern void *calloc(size_t nmemb, size_t size);
extern void *realloc(void *ptr, size_t size);

/* Conversion functions */
int atoi(const char *s);
long atol(const char *s);
long strtol(const char *s, char **endptr, int base);
unsigned long strtoul(const char *s, char **endptr, int base);

/* Misc */
void abort(void);

#endif /* _STDLIB_H */
