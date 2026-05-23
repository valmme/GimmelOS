#pragma once
#include "lib/types.h"

// asm
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);

void outb(uint16_t port, uint8_t data);
void outw(uint16_t, uint16_t data);

void halt(void);

// cursor
typedef struct {
    int32_t x;
    int32_t y;
    uint8_t left;
    uint8_t right;
    uint8_t middle;
} mouse_state_t;

extern mouse_state_t mouse;

void mouse_init(void);
void mouse_poll(void);