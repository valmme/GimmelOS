#include "drivers/keyboard.h"

#define KBD_QUEUE_SIZE 32

static uint8_t kbd_queue[KBD_QUEUE_SIZE];
static int kbd_q_head  = 0;
static int kbd_q_tail  = 0;
static int kbd_enabled = 1;

static uint8_t key_down[256];
static uint8_t key_pressed[256];

static const char sc_ascii[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o',
    'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'', '`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',',
    '.', '/', 0,
    '*',
    0,
    ' ',
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,
    '7','8','9','-',
    '4','5','6','+',
    '1','2','3','0',
    '.',
};

static const char sc_shifted[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O',
    'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
    ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<',
    '>', '?', 0,
    '*',
    0,
    ' ',
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,
    '7','8','9','-',
    '4','5','6','+',
    '1','2','3','0',
    '.',
};

static int shift_held = 0;
static int ctrl_held  = 0;
static int caps_lock  = 0;
static int kbd_pending_e0 = 0;

static int kbd_has_data(void) {
    uint8_t status = inb(KBD_STATUS_PORT);
    return (status & 0x01) && !(status & 0x20);
}

void keyboard_init(void) {
    while (inb(KBD_STATUS_PORT) & 0x01)
        inb(KBD_DATA_PORT);
}

int keyboard_haskey(void) {
    return kbd_has_data();
}

static int process_scancode(uint8_t sc) {
    if (sc == SC_LSHIFT || sc == SC_RSHIFT) { shift_held = 1; return -1; }
    if (sc == SC_CTRL)                       { ctrl_held  = 1; return -1; }
    if (sc == SC_CAPS)                       { caps_lock  = !caps_lock; return -1; }

    if (sc & KEY_RELEASE) {
        uint8_t rel = sc & ~KEY_RELEASE;
        if (rel == SC_LSHIFT || rel == SC_RSHIFT) shift_held = 0;
        if (rel == SC_CTRL)                        ctrl_held  = 0;
        return -1;
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

    if (sc >= 128) return -1;

    char c = shift_held ? sc_shifted[sc] : sc_ascii[sc];
    if (caps_lock && c >= 'a' && c <= 'z') c -= 32;
    if (caps_lock && c >= 'A' && c <= 'Z' && shift_held) c += 32;
    if (ctrl_held) {
        if (c == 's' || c == 'S') return 19;
        if (c == 'z' || c == 'Z') return 26;
    }

    return c ? (int)c : -1;
}

int keyboard_getchar(void) {
    while (1) {
        while (!kbd_has_data());
        uint8_t sc = inb(KBD_DATA_PORT);

        if (sc == 0xE0) {
            while (!kbd_has_data());
            uint8_t ext = inb(KBD_DATA_PORT);
            switch (ext) {
                case 0x48: return KEY_UP;
                case 0x50: return KEY_DOWN;
                case 0x4B: return KEY_LEFT;
                case 0x4D: return KEY_RIGHT;
            }
            continue;
        }

        int r = process_scancode(sc);
        if (r >= 0) return r;
    }
}

void keyboard_push_scancode(uint8_t sc) {
    uint8_t key = sc & 0x7F;

    if (sc & 0x80) {
        key_down[key] = 0;
    } else {
        if (!key_down[key])
            key_pressed[key] = 1;

        key_down[key] = 1;
    }

    int next = (kbd_q_tail + 1) % KBD_QUEUE_SIZE;

    if (next != kbd_q_head) {
        kbd_queue[kbd_q_tail] = sc;
        kbd_q_tail = next;
    }
}

uint8_t keyboard_is_key_down(uint8_t scancode) {
    return key_down[scancode];
}

uint8_t keyboard_is_key_pressed(uint8_t scancode) {
    return key_pressed[scancode];
}

void keyboard_set_enabled(int e) {
    kbd_enabled = e;
}

int keyboard_getchar_nonblocking(void) {
    if (!kbd_enabled) return 0;
    if (kbd_q_head == kbd_q_tail) return 0;

    uint8_t sc = kbd_queue[kbd_q_head];
    kbd_q_head = (kbd_q_head + 1) % KBD_QUEUE_SIZE;

    if (sc == 0xE0) {
        kbd_pending_e0 = 1;
        return 0;
    }

    if (kbd_pending_e0) {
        kbd_pending_e0 = 0;
        switch (sc) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
        }
        return 0;
    }

    int r = process_scancode(sc);
    return (r >= 0) ? r : 0;
}