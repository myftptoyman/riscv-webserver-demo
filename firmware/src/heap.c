#include "heap.h"
#include <stdint.h>
#include <string.h>

extern char __heap_start[];
extern char __heap_end[];

/* Block header for free-list allocator */
typedef struct block_header {
    size_t size;
    struct block_header *next;
    int free;
} block_header_t;

#define HEADER_SIZE sizeof(block_header_t)
#define ALIGN_SIZE 16

static block_header_t *heap_start = NULL;

/* Align size to 16 bytes */
static inline size_t align_up(size_t size) {
    return (size + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1);
}

void heap_init(void) {
    heap_start = (block_header_t*)__heap_start;
    heap_start->size = (size_t)(__heap_end - __heap_start) - HEADER_SIZE;
    heap_start->next = NULL;
    heap_start->free = 1;
}

void *malloc(size_t size) {
    if (size == 0) return NULL;

    size = align_up(size);

    block_header_t *current = heap_start;
    block_header_t *prev = NULL;

    while (current != NULL) {
        if (current->free && current->size >= size) {
            /* Found suitable block */
            if (current->size >= size + HEADER_SIZE + ALIGN_SIZE) {
                /* Split block */
                block_header_t *new_block = (block_header_t*)((char*)current + HEADER_SIZE + size);
                new_block->size = current->size - size - HEADER_SIZE;
                new_block->next = current->next;
                new_block->free = 1;

                current->size = size;
                current->next = new_block;
            }
            current->free = 0;
            return (char*)current + HEADER_SIZE;
        }
        prev = current;
        current = current->next;
    }

    return NULL; /* Out of memory */
}

void free(void *ptr) {
    if (ptr == NULL) return;

    block_header_t *block = (block_header_t*)((char*)ptr - HEADER_SIZE);
    block->free = 1;

    /* Coalesce with next block if free */
    if (block->next && block->next->free) {
        block->size += HEADER_SIZE + block->next->size;
        block->next = block->next->next;
    }

    /* Coalesce with previous block if free */
    block_header_t *current = heap_start;
    while (current != NULL && current->next != block) {
        current = current->next;
    }
    if (current && current->free) {
        current->size += HEADER_SIZE + block->size;
        current->next = block->next;
    }
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    block_header_t *block = (block_header_t*)((char*)ptr - HEADER_SIZE);
    if (block->size >= size) {
        return ptr; /* Current block is large enough */
    }

    void *new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        free(ptr);
    }
    return new_ptr;
}
