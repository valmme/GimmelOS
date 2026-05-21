#include "keyboard.h"
#include "kernel/cpu/io.h"
#include "lib/types.h"
 
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
static int ctrl_held = 0;
static int caps_lock = 0;

void keyboard_init(void) {
    while (inb(KBD_STATUS_PORT) & 0x01) 
        inb(KBD_DATA_PORT);
}

int keyboard_haskey(void) {
    return (inb(KBD_STATUS_PORT) & 0x01) != 0;
}

int keyboard_getchar(void) {
    while (1) {
        if (!keyboard_haskey()) continue;

        uint8_t sc = inb(KBD_DATA_PORT);

        if (sc == 0xE0) {
            uint8_t ext = inb(KBD_DATA_PORT);

            switch (ext) {
                case 0x48: return KEY_UP;
                case 0x50: return KEY_DOWN;
                case 0x4B: return KEY_LEFT;
                case 0x4D: return KEY_RIGHT;
            }

            continue;
        }

        if (sc & KEY_RELEASE) {
            uint8_t rel = sc & ~KEY_RELEASE;

            if (rel == SC_LSHIFT || rel == SC_RSHIFT)
                shift_held = 0;

            if (rel == SC_CTRL)
                ctrl_held = 0;

            continue;
        }

        if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
            shift_held = 1;
            continue;
        }

        if (sc == SC_CTRL) {
            ctrl_held = 1;
            continue;
        }

        if (sc == SC_CAPS) {
            caps_lock = !caps_lock;
            continue;
        }

        switch (sc) {
            case 0x47: return '7';
            case 0x48: return '8';
            case 0x49: return '9';
            case 0x4B: return '4';
            case 0x4C: return '5';
            case 0x4D: return '6';
            case 0x4F: return '1';
            case 0x50: return '2';
            case 0x51: return '3';
            case 0x52: return '0';

            case 0x53: return '.';
            case 0x4A: return '-';
            case 0x4E: return '+';
            case 0x37: return '*';
            case 0x35: return '/';
        }

        char c = shift_held ? sc_shifted[sc] : sc_ascii[sc];

        if (caps_lock && c >= 'a' && c <= 'z') c -= 32;
        if (caps_lock && c >= 'A' && c <= 'Z' && shift_held)c += 32;

        if (ctrl_held) {
            if (c == 's' || c == 'S') return 19;
            if (c == 'z' || c == 'Z') return 26;
        }

        if (c) return c;
    }
}