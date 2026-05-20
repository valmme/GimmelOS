#include "heap.h"
#define HEAP_SIZE 1024 * 1024

static int8_t heap[HEAP_SIZE];
static uint32_t heap_top = 0;

void heap_init() {
    heap_top = 0;
}

void* kmalloc(uint32_t size) {
    if (heap_top + size >= HEAP_SIZE) {
        return 0;
    }

    void* ptr = &heap[heap_top];
    heap_top += size;

    return ptr;
}