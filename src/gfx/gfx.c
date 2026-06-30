#include "gfx/gfx.h"
#include "gfx/wm.h"
#include "gfx/icons.h"
#include "gfx/startup.h"
#include "lib/kstring.h"

uint32_t* framebuffer;
uint32_t width;
uint32_t height;
uint32_t pitch;

static uint32_t fb_cursor_x = 0;
static uint32_t fb_cursor_y = 0;

static gfx_color_t desktop_color_top_left = {74, 117, 247, 255};
static gfx_color_t desktop_color_bottom_right = {57, 237, 168, 255};

static rec gfx_clip;

const uint32_t vga_palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

uint32_t* backbuffer = 0;

const glyph_t* unifont_get(uint32_t cp) {
    int l = 0;
    int r = unifont_count - 1;

    while (l <= r) {
        int m = (l + r) / 2;

        if (unifont[m].codepoint == cp)
            return &unifont[m];

        if (unifont[m].codepoint < cp)
            l = m + 1;
        
        else
            r = m - 1;
    }

    return 0;
}

void gfx_init(vec2 res, uint32_t p, uint32_t* fb) {
    width = res.x;
    height = res.y;

    pitch = p;
    framebuffer = fb;

    static uint32_t _backbuffer[1920 * 1080];
    backbuffer = _backbuffer;
}

void gfx_begin_frame(gfx_color_t color) {
    for (int i = 0; i < width * height; i++) {
        backbuffer[i] = ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | (uint32_t)color.b;
    }
}

void gfx_end_frame(void) {
    uint8_t* dst = (uint8_t*)framebuffer;

    for (uint32_t y = 0; y < height; y++) {
        uint32_t* drow = (uint32_t*)(dst + y * pitch);
        uint32_t* srow = backbuffer + y * width;

        for (uint32_t x = 0; x < width; x++)
            drow[x] = srow[x];
    }
}

void gfx_paint_desktop(void) {
    rec screen_rec = {0, 0, width, height};
    wm98_draw_diagonal_dither_gradient(screen_rec, desktop_color_top_left, desktop_color_bottom_right);   
}

void gfx_put_pixel(uint32_t x, uint32_t y, gfx_color_t color) {
    backbuffer[y * width + x] = gfx_pack_color(color);
}

void gfx_putchar_ex(char c, vec2 pos, gfx_color_t fg, int scale) {
    const glyph_t* glyph = unifont_get((uint8_t)c);
    if (!glyph) return;

    for (int row = 0; row < FB_CHAR_H; row++) {
        uint8_t bits = glyph->bitmap[row];

        for (int col = 0; col < FB_CHAR_W; col++) {
            int is_fg = bits & (0x80 >> col);
            if (!is_fg) continue;

            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    gfx_put_pixel(
                        pos.x + col * scale + sx,
                        pos.y + row * scale + sy,
                        fg
                    );
                }
            }
        }
    }
}

void gfx_putchar(char c, vec2 pos, gfx_color_t fg) {
    gfx_putchar_ex(c, pos, fg, 1);
}

void gfx_print_ex(const char* str, vec2 pos, gfx_color_t fg, int scale) {
    int start_x = pos.x;
    while (*str) {
        if (*str == '\n') {
            pos.y += FB_CHAR_H * scale;
            pos.x  = start_x;
        } 
        
        else {
            gfx_putchar_ex(*str, pos, fg, scale);
            pos.x += FB_CHAR_W * scale;
        }
        str++;
    }
}

void gfx_print(const char* str, vec2 pos, gfx_color_t fg) {
    gfx_print_ex(str, pos, fg, 1);
}

void gfx_draw_line(vec2 a, vec2 b, gfx_color_t color) {
    int dx = (b.x > a.x) ? b.x - a.x : a.x - b.x;
    int sx = (a.x < b.x) ? 1 : -1;

    int dy = (b.y > a.y) ? b.y - a.y : a.y - b.y;
    int sy = (a.y < b.y) ? 1 : -1;

    int err = dx - dy;

    while (1) {
        gfx_put_pixel(a.x, a.y, color);

        if (a.x == b.x && a.y == b.y) break;

        int e2 = 2 * err;

        if (e2 > -dy) {
            err -= dy;
            a.x += sx;
        }

        if (e2 < dx) {
            err += dx;
            a.y += sy;
        }
    }
}

void gfx_draw_rec(rec r, gfx_color_t color) {
    // top, bottom, left, right
    for (int32_t x = r.x; x < r.x + (int32_t)r.w; x++) { gfx_put_pixel(x, r.y, color); }
    for (int32_t x = r.x; x < r.x + (int32_t)r.w; x++) { gfx_put_pixel(x, r.y + (int32_t)r.h - 1, color); }
    for (int32_t y = r.y; y < r.y + (int32_t)r.h; y++) { gfx_put_pixel(r.x, y, color); }
    for (int32_t y = r.y; y < r.y + (int32_t)r.h; y++) { gfx_put_pixel(r.x + (int32_t)r.w - 1, y, color); }
}

void gfx_draw_fill_rec(rec r, gfx_color_t color) {
    int32_t x0 = r.x < 0 ? 0 : r.x;
    int32_t y0 = r.y < 0 ? 0 : r.y;
    int32_t x1 = r.x + r.w;
    int32_t y1 = r.y + r.h;
    
    if (x1 > width) x1 = width;
    if (y1 > height) y1 = height;
    if (x0 >= x1 || y0 >= y1) return;

    uint32_t packed = ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | (uint32_t)color.b;

    for (int32_t y = y0; y < y1; y++) {
        uint32_t* row = backbuffer + y * width;
        for (int32_t x = x0; x < x1; x++) {
            row[x] = packed;
        }
    }
}

static void cursor_save_bg(int32_t x, int32_t y) {
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            int sx = x + col, sy = y + row;

            if (sx < 0 || sy < 0 || (uint32_t)sx >= width || (uint32_t)sy >= height)
                cursor_bg[row * CURSOR_W + col] = 0;

            else
                cursor_bg[row * CURSOR_W + col] = *((uint32_t*)((uint8_t*)framebuffer + sy * pitch + sx * 4));
        }
    }
}

static void cursor_restore_bg(int32_t x, int32_t y) {
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            int sx = x + col, sy = y + row;
            if (sx < 0 || sy < 0 || (uint32_t)sx >= width || (uint32_t)sy >= height)
                continue;

            *((uint32_t*)((uint8_t*)framebuffer + sy * pitch + sx * 4)) = cursor_bg[row * CURSOR_W + col];
        }
    }
}

void gfx_draw_cursor(mouse_state_t mouse) {
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            if (cursor_bitmap[row] & (1u << (31 - col)))
                gfx_put_pixel(mouse.pos.x + col, mouse.pos.y + row, GFX_WHITE);
        }
    }
}

void gfx_draw_texture_ex(const uint32_t* tex, vec2 pos, vec2 src_size, vec2 dst_size, gfx_color_t color) {
    int32_t start_x = pos.x;
    int32_t start_y = pos.y;
    int32_t end_x = start_x + (int32_t)dst_size.x;
    int32_t end_y = start_y + (int32_t)dst_size.y;

    if (end_x > (int32_t)width)  end_x = (int32_t)width;
    if (end_y > (int32_t)height) end_y = (int32_t)height;
    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;

    for (int32_t y = start_y; y < end_y; y++) {
        uint32_t* row_ptr = &backbuffer[y * width];
        uint32_t src_y = ((y - pos.y) * (uint32_t)src_size.y) / (uint32_t)dst_size.y;

        for (int32_t x = start_x; x < end_x; x++) {
            uint32_t src_x = ((x - pos.x) * (uint32_t)src_size.x) / (uint32_t)dst_size.x;
            uint32_t tex_pixel = tex[src_y * (uint32_t)src_size.x + src_x];

            uint8_t a = (tex_pixel >> 24) & 0xFF;
            if (a == 0) continue;

            uint8_t r = (tex_pixel >> 16) & 0xFF;
            uint8_t g = (tex_pixel >> 8) & 0xFF;
            uint8_t b = tex_pixel & 0xFF;

            uint8_t final_a = (uint8_t)((a * color.a) / 255);
            if (final_a == 0) continue;

            uint8_t sr, sg, sb;

            if (r == g && g == b && r != 0) {
                sr = color.r;
                sg = color.g;
                sb = color.b;
            } 
            
            else {
                sr = r;
                sg = g;
                sb = b;
            }

            uint32_t dst = row_ptr[x];
            uint8_t dr = (dst >> 16) & 0xFF;
            uint8_t dg = (dst >> 8) & 0xFF;
            uint8_t db = dst & 0xFF;

            uint8_t out_r = (sr * final_a + dr * (255 - final_a)) / 255;
            uint8_t out_g = (sg * final_a + dg * (255 - final_a)) / 255;
            uint8_t out_b = (sb * final_a + db * (255 - final_a)) / 255;

            row_ptr[x] =
                (0xFF << 24) |
                ((uint32_t)out_r << 16) |
                ((uint32_t)out_g << 8) |
                (uint32_t)out_b;
        }
    }
}

void gfx_draw_texture(const uint32_t* tex, vec2 pos, vec2 src_size, vec2 dst_size) {
    int32_t start_x = pos.x;
    int32_t start_y = pos.y;
    int32_t end_x = start_x + (int32_t)dst_size.x;
    int32_t end_y = start_y + (int32_t)dst_size.y;

    if (end_x > (int32_t)width)  end_x = (int32_t)width;
    if (end_y > (int32_t)height) end_y = (int32_t)height;
    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;

    for (int32_t y = start_y; y < end_y; y++) {
        uint32_t* row_ptr = &backbuffer[y * width];
        uint32_t src_y = ((y - pos.y) * (uint32_t)src_size.y) / (uint32_t)dst_size.y;

        for (int32_t x = start_x; x < end_x; x++) {
            uint32_t src_x = ((x - pos.x) * (uint32_t)src_size.x) / (uint32_t)dst_size.x;
            uint32_t tex_pixel = tex[src_y * (uint32_t)src_size.x + src_x];

            uint8_t a = (tex_pixel >> 24) & 0xFF;
            if (a == 0) continue;

            row_ptr[x] = tex_pixel;
        }
    }
}

void gfx_set_clip(rec r) {
    gfx_clip = r;
}

void gfx_reset_clip(void) {
    gfx_clip = (rec){0, 0, width, height};
}

void gfx_put_pixel_clipped(vec2 pos, gfx_color_t color) {
    if (pos.x < gfx_clip.x || pos.x >= gfx_clip.x + (int32_t)gfx_clip.w) return;
    if (pos.y < gfx_clip.y || pos.y >= gfx_clip.y + (int32_t)gfx_clip.h) return;
    gfx_put_pixel(pos.x, pos.y, color);
}