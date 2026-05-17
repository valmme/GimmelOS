#pragma once
#include "types.h"

typedef enum {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_LIGHT_BROWN, VGA_WHITE
} vga_color_t;

void vga_init(void);
void vga_clear(void);
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_putchar(char c);
void vga_print(const char* str);
void vga_println(const char* str);
void vga_print_hex(uint32_t value);
void vga_print_int(int32_t value);
void vga_set_cursor(size_t row, size_t col);