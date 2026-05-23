#pragma once
#include "lib/types.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

typedef enum {
    VGA_BLACK          = 0,
    VGA_BLUE           = 1,
    VGA_GREEN          = 2,
    VGA_CYAN           = 3,
    VGA_RED            = 4,
    VGA_MAGENTA        = 5,
    VGA_BROWN          = 6,
    VGA_LIGHT_GREY     = 7,
    VGA_DARK_GREY      = 8,
    VGA_LIGHT_BLUE     = 9,
    VGA_LIGHT_GREEN    = 10,
    VGA_LIGHT_CYAN     = 11,
    VGA_LIGHT_RED      = 12,
    VGA_LIGHT_MAGENTA  = 13,
    VGA_LIGHT_BROWN    = 14,
    VGA_WHITE          = 15,
} vga_color_t;

void vga_init(void);
void vga_clear(void);
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_set_cursor(size_t row, size_t col);
void vga_put_at(char c, int row, int col);

void vga_putchar(char c);
void vga_print(const char* str);
void vga_println(const char* str);
void vga_printf(const char* fmt, ...);

void vga_print_hex(uint32_t value);
void vga_print_uint(uint32_t value);
void vga_print_int(int32_t value);

void vga_error(const char* str);
void vga_success(const char* str);
void vga_info(const char* str);
void vga_warn(const char* str);

// graphics