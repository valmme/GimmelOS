#ifndef GOS_HEAP_H
#define GOS_HEAP_H

#include "lib/types.h"

typedef struct heap_block {
    uint32_t size;
    uint8_t free;
    struct heap_block *next;
} heap_block_t;

void heap_init(void* addr, uint32_t size);
void* kmalloc(uint32_t size);
void kfree(void* ptr);

#endif // GOS_HEAP_H