#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

void heap_init(void);
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

#endif /* HEAP_H */
