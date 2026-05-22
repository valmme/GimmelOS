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

extern uint32_t* framebuffer;
extern uint32_t width;
extern uint32_t height;
extern uint32_t pitch;

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;

    uint32_t boot_device;
    uint32_t cmdline;

    uint32_t mods_count;
    uint32_t mods_addr;

    uint32_t syms[4];

    uint32_t mmap_length;
    uint32_t mmap_addr;

    uint32_t drives_length;
    uint32_t drives_addr;

    uint32_t config_table;

    uint32_t boot_loader_name;

    uint32_t apm_table;

    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    uint32_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
};

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
void gfx_put_pixel(uint32_t x, uint32_t y, vga_color_t color);
void gfx_line(vec2 a, vec2 b, vga_color_t color);
void gfx_clear(vga_color_t color);
void gfx_render_frame();