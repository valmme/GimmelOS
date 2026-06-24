#include "kernel/io.h"
#include "drivers/keyboard.h"
#include "gfx/gfx.h"

#define MOUSE_QUEUE 16

static uint8_t mouse_buf[3];
static int mouse_buf_pos = 0;
static unsigned long seed = 0;

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

uint64_t rdtsc(void) {
    uint64_t ret;
    __asm__ __volatile__ ("rdtsc" : "=A" (ret));
    return ret;
}

void halt(void) {
    __asm__ volatile ("cli; hlt");
}

// cursor
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
    mouse.pos.y = 300;
}

void io_poll(void) {
    uint8_t status;

    while ((status = inb(0x64)) & 0x01) {
        uint8_t data = inb(0x60);

        if (status & 0x20) {
            mouse_buf[mouse_buf_pos++] = data;

            if (mouse_buf_pos == 1 && !(data & 0x08))
                mouse_buf_pos = 0;

            if (mouse_buf_pos == 3) {
                uint8_t flags = mouse_buf[0];

                if (!(flags & 0x40) && !(flags & 0x80)) {
                    mouse.left   = (flags & 0x01) != 0;
                    mouse.right  = (flags & 0x02) != 0;
                    mouse.middle = (flags & 0x04) != 0;

                    mouse.pos.x += (int32_t)(int8_t)mouse_buf[1];
                    mouse.pos.y -= (int32_t)(int8_t)mouse_buf[2];

                    if (mouse.pos.x < 0) mouse.pos.x = 0;
                    if (mouse.pos.y < 0) mouse.pos.y = 0;
                    if ((uint32_t)mouse.pos.x >= width)  mouse.pos.x = width  - 1;
                    if ((uint32_t)mouse.pos.y >= height) mouse.pos.y = height - 1;
                }

                mouse_buf_pos = 0;
            }
        } 
        
        else {
            keyboard_push_scancode(data);
        }
    }
}

// random
void random_init(void) {
    seed = (unsigned int)rdtsc();
}

unsigned int get_random(void) {
    seed = seed * 1103515245 + 12345;
    return (unsigned int)(seed / 65536) % 32768;
}