#include "io.h"
#include "gfx/gfx.h"

mouse_state_t mouse = {0};

uint8_t inb(uint16_t port) {
    uint8_t data;
    __asm__ volatile ("inb %1, %0" : "=a"(data) : "d"(port));
    return data;
}

uint16_t inw(uint16_t port) {
    uint16_t data;
    __asm__ volatile ("inw %1, %0" : "=a"(data) : "d"(port));
    return data;
}

void outb(uint16_t port, uint8_t data) {
    __asm__ volatile ("outb %0, %1" :: "a"(data), "d"(port));
}

void outw(uint16_t port, uint16_t data) {
    __asm__ volatile ("outw %0, %1" :: "a"(data), "d"(port));
}

void halt(void) {
    __asm__ volatile ("cli; hlt");
}

static void mouse_wait_write(void) {
    while (inb(0x64) & 0x02);
}

static void mouse_wait_read(void) {
    while (!(inb(0x64) & 0x01));
}

static void mouse_write(uint8_t val) {
    mouse_wait_write();
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, val);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(0x60);
}

void mouse_init(void) {
    mouse_wait_write();
    outb(0x64, 0xA8);

    // interrupt
    mouse_wait_write();
    outb(0x64, 0x20);
    mouse_wait_read();

    uint8_t status = inb(0x60) | 0x02;

    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();

    mouse.pos.x = 400;
    mouse.pos.y =  300;
}

void mouse_poll(void) {
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) return; // no data
    if (!(status & 0x20)) return; // data from keyboard

    uint8_t flags = inb(0x60);

    mouse_wait_read();
    int32_t dx = (int32_t)(int8_t)inb(0x60);
    
    mouse_wait_read();
    int32_t dy = (int32_t)(int8_t)inb(0x60);

    if (flags & 0x40 || flags & 0x80) return;

    mouse.left   = (flags & 0x01) != 0;
    mouse.right  = (flags & 0x02) != 0;
    mouse.middle = (flags & 0x04) != 0;

    mouse.pos.x += dx;
    mouse.pos.y -= dy;

    if (mouse.pos.x < 0) mouse.pos.x = 0;
    if (mouse.pos.y < 0) mouse.pos.y = 0;
    if ((uint32_t)mouse.pos.x >= width)  mouse.pos.x = (int32_t)width  - 1;
    if ((uint32_t)mouse.pos.y >= height) mouse.pos.y = (int32_t)height - 1;
}