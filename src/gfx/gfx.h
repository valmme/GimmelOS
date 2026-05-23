#pragma once
#include "kernel/cpu/io.h"
#include "drivers/vga/vga.h"
#include "lib/types.h"
#include "ui/wm.h"

#define FB_CHAR_W 8
#define FB_CHAR_H 8

extern uint32_t* framebuffer;
extern uint32_t width;
extern uint32_t height;
extern uint32_t pitch;

extern const uint32_t vga_palette[16];

struct __attribute__((packed)) multiboot_info {
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
    uint32_t framebuffer_addr_low;
    uint32_t framebuffer_addr_high;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
};

extern const uint8_t font[128][8];

// colors
#define GFX_LIGHT_GRAY  (gfx_color_t){ 200, 200, 200, 255 }
#define GFX_DARK_GRAY   (gfx_color_t){  80,  80,  80, 255 }
#define GFX_GRAY        (gfx_color_t){ 130, 130, 130, 255 }
#define GFX_YELLOW      (gfx_color_t){ 250, 250,   0, 255 }
#define GFX_GOLD        (gfx_color_t){ 255, 200,   0, 255 }
#define GFX_ORANGE      (gfx_color_t){ 255, 160,   0, 255 }
#define GFX_PINK        (gfx_color_t){ 255, 100, 200, 255 }
#define GFX_RED         (gfx_color_t){ 255,   0,   0, 255 }
#define GFX_MAROON      (gfx_color_t){ 190,  30,  50, 255 }
#define GFX_GREEN       (gfx_color_t){   0, 255,   0, 255 }
#define GFX_LIME        (gfx_color_t){   0, 160,  50, 255 }
#define GFX_DARK_GREEN  (gfx_color_t){   0, 120,  40, 255 }
#define GFX_SKY_BLUE    (gfx_color_t){ 100, 190, 255, 255 }
#define GFX_BLUE        (gfx_color_t){   0,   0, 255, 255 }
#define GFX_DARK_BLUE   (gfx_color_t){   0,  80, 170, 255 }
#define GFX_PURPLE      (gfx_color_t){ 200, 120, 255, 255 }
#define GFX_VIOLET      (gfx_color_t){ 135,  60, 190, 255 }
#define GFX_DARK_PURPLE (gfx_color_t){ 110,  30, 125, 255 }
#define GFX_BEIGE       (gfx_color_t){ 210, 180, 130, 255 }
#define GFX_BROWN       (gfx_color_t){ 130, 100,  80, 255 }
#define GFX_DARK_BROWN  (gfx_color_t){  80,  60,  50, 255 }

#define GFX_WHITE       (gfx_color_t){ 255, 255, 255, 255 }
#define GFX_BLACK       (gfx_color_t){   0,   0,   0, 255 }
#define GFX_BLANK       (gfx_color_t){   0,   0,   0, 0 }
#define GFX_MAGENTA     (gfx_color_t){ 255,   0, 255, 255 }


void gfx_init(vec2 res, uint32_t p, uint32_t* fb);

// basic
gfx_color_t gfx_from_vga(vga_color_t c);
void gfx_begin_frame(gfx_color_t color);
void gfx_end_frame(void);
void gfx_put_pixel(uint32_t x, uint32_t y, gfx_color_t color);
void gfx_draw_cursor(mouse_state_t mouse);

// text
void gfx_putchar_ex(char c, vec2 pos, gfx_color_t fg, gfx_color_t bg, int scale);
void gfx_putchar(char c, vec2 pos, gfx_color_t fg, gfx_color_t bg);
void gfx_print_ex(const char* str, vec2 pos, gfx_color_t fg, gfx_color_t bg, int scale);
void gfx_print(const char* str, vec2 pos, gfx_color_t fg, gfx_color_t bg);

// shapes
void gfx_draw_line(vec2 a, vec2 b, gfx_color_t color);
void gfx_draw_rec(rec r, gfx_color_t color);
void gfx_draw_fill_rec(rec r, gfx_color_t color);

void gfx_draw_texture(const uint32_t* tex, vec2 pos, vec2 size);