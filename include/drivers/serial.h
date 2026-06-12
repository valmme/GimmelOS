#ifndef GOS_SERIAL_H
#define GOS_SERIAL_H

#include "kernel/cpu/io.h"

static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
}

static void serial_putchar(char c) {
    while (!(inb(0x3FD) & 0x20));
    outb(0x3F8, c);
}

static void serial_print(const char* s) {
    while (*s) serial_putchar(*s++);
}

static void serial_print_uint(uint32_t v) {
    char buf[10]; int i = 0;
    if (v == 0) { serial_putchar('0'); return; }
    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i--) serial_putchar(buf[i]);
}

static void serial_print_hex(uint32_t v) {
    const char hex[] = "0123456789ABCDEF";
    serial_print("0x");
    
    for (int i = 7; i >= 0; i--)
        serial_putchar(hex[(v >> (i * 4)) & 0xF]);
}

#endif // GOS_SERIAL_H