#ifndef GOS_IO_H
#define GOS_IO_H

#include "lib/types.h"

// asm
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);

void outb(uint16_t port, uint8_t data);
void outw(uint16_t, uint16_t data);

uint64_t rdtsc(void);

void halt(void);

// cursor
typedef struct {
    vec2 pos;
    vec2 delta;
    uint8_t left;
    uint8_t right;
    uint8_t middle;
} mouse_state_t;

extern mouse_state_t mouse;

void mouse_init(void);
void io_poll(void);

// random
void random_init(void);
unsigned int get_random(void);

#endif // GOS_IO_H