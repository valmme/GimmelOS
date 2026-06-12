#ifndef GOS_KEYBOARD_H
#define GOS_KEYBOARD_H

#include "lib/types.h"

#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CTRL     0x1D
#define SC_CAPS     0x3A


#define KEY_UP      1001
#define KEY_DOWN    1002
#define KEY_LEFT    1003
#define KEY_RIGHT   1004
#define KEY_RELEASE 0x80

#define KEY_ESCAPE  '\x1B'
#define KEY_TAB     '\x09'

void keyboard_init(void);
int keyboard_getchar(void);
int keyboard_haskey(void);
void keyboard_handler(void);
int keyboard_getchar_nonblocking(void);
void keyboard_push_scancode(uint8_t sc);

#endif // GOS_KEYBOARD_H