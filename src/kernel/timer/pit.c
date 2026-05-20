#include "pit.h"

static uint32_t ticks = 0;

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}

void pit_handler() {
    ticks++;
    outb(0x20, 0x20);
}

void pit_init(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;

    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

uint32_t pit_ticks() {
    return ticks;
}