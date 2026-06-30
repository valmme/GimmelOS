#include "heap.h"

static heap_block_t* heap_head = 0;

void heap_init(void* addr, uint32_t size) {
    heap_head = (heap_block_t*)addr;

    heap_head->size = size - sizeof(heap_block_t);
    heap_head->free = 1;
    heap_head->next = 0;
}

void* kmalloc(uint32_t size) {
    if (size == 0)
        return 0;

    heap_block_t* block = heap_head;

    while (block) {
        if (block->free && block->size >= size) {
            if (block->size >= size + sizeof(heap_block_t) + 8) {
                heap_block_t* new_block = (heap_block_t*)((uint8_t*)block + sizeof(heap_block_t) + size);

                new_block->size = block->size - size - sizeof(heap_block_t);
                new_block->free = 1;
                new_block->next = block->next;

                block->next = new_block;
                block->size = size;
            }

            block->free = 0;
            return (uint8_t*)block + sizeof(heap_block_t);
        }

        block = block->next;
    }

    return 0;
}

void kfree(void* ptr) {
    if (!ptr)
        return;

    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));

    block->free = 1;
    heap_block_t* cur = heap_head;

    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += sizeof(heap_block_t) + cur->next->size;
            cur->next = cur->next->next;
        }

        else {
            cur = cur->next;
        }
    }
}