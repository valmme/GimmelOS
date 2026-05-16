#include "keyboard.h"
 
#define KBD_DATA_PORT  0x60
#define KBD_STATUS_PORT 0x64

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
 
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
 
static const char sc_ascii[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'','`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' '
};
 
static const char sc_shifted[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' '
};

static int shift_held = 0;

void keyboard_init(void) {
    while (inb(KBD_STATUS_PORT) & 0x01) 
        inb(KBD_DATA_PORT);
}

int keyboard_haskey(void) {
    return (inb(KBD_STATUS_PORT) & 0x01) != 0;
}

char keyboard_getchar(void) {
    while (1) {
        if (!keyboard_haskey()) continue;

        uint8_t sc = inb(KBD_DATA_PORT);

        if (sc & 0x80) {
            uint8_t rel = sc & 0x7F;
            if (rel == SC_LSHIFT || rel == SC_RSHIFT) shift_held = 0;
            continue;
        }

        if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
            shift_held = 1;
            continue;
        }

        char c = shift_held ? sc_shifted[sc] : sc_ascii[sc];
        if (c) return c;
    }
}


