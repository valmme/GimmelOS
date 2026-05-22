#include "vga.h"
#include "kernel/cpu/io.h"

#define VGA_MEM ((uint16_t *)0xB8000)

static size_t vga_row = 0;
static size_t vga_col = 0;
static uint8_t vga_attr = 0;

uint32_t* framebuffer;
uint32_t width;
uint32_t height;
uint32_t pitch;

// helpers
static uint16_t vga_entry(char c, uint8_t attr) {
    return (uint16_t)c | ((uint16_t)attr << 8);
}

void vga_put_at(char c, int row, int col) {
    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) return;
    VGA_MEM[row * VGA_WIDTH + col] = vga_entry(c, vga_attr);
}

static void update_hw_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_WIDTH + vga_col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8) & 0xFF);
}

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}
 
static uint8_t bcd_to_bin(uint8_t v) {
    return (v & 0x0F) + ((v >> 4) * 10);
}
 
static uint16_t read_year(void) {
    uint8_t y = bcd_to_bin(cmos_read(0x09));
    return (y < 70) ? 2000 + y : 1900 + y;
}

static void get_date(uint8_t *d, uint8_t *m, uint16_t *y) {
    *d = bcd_to_bin(cmos_read(0x07));
    *m = bcd_to_bin(cmos_read(0x08));
    *y = read_year();
}
 
static void get_time(uint8_t *h, uint8_t *m, uint8_t *s) {
    *s = bcd_to_bin(cmos_read(0x00));
    *m = bcd_to_bin(cmos_read(0x02));
    *h = bcd_to_bin(cmos_read(0x04));
}

static void scroll(void) {
    for (size_t r = 1; r < VGA_HEIGHT; r++) {
        for (size_t c = 0; c < VGA_WIDTH; c++) {
            VGA_MEM[(r-1) * VGA_WIDTH + c] = VGA_MEM[r * VGA_WIDTH + c];
        }
    }

    for (size_t c = 0; c < VGA_WIDTH; c++) {
        VGA_MEM[(VGA_HEIGHT-1) * VGA_WIDTH + c] = vga_entry(' ', vga_attr);
    }

    vga_row = VGA_HEIGHT - 1;
}

// vga base
void vga_init(void) {
    vga_attr = (uint8_t)((VGA_BLACK << 4) | VGA_LIGHT_GREY);
    vga_clear();
    vga_info("VGA Initialized");
}

void vga_clear(void) {
    for (size_t r = 0; r < VGA_HEIGHT; r++) {
        for (size_t c = 0; c < VGA_WIDTH; c++) {
            VGA_MEM[r * VGA_WIDTH + c] = vga_entry(' ', vga_attr);
        }
    }

    vga_row = 0;
    vga_col = 0;
    update_hw_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    vga_attr = (uint8_t)(((uint8_t)bg << 4) | (uint8_t)fg);
}

void vga_set_cursor(size_t row, size_t col) {
    vga_row = row; vga_col = col;
    update_hw_cursor();
}

void vga_putchar(char c) {
    switch (c) {
        case '\n':
            vga_col = 0;
            if (++vga_row == VGA_HEIGHT) scroll();
            break;
        
        case '\r':
            vga_col = 0;
            break;
        
        case '\b':
            if (vga_col > 0) {
                vga_col--;
                VGA_MEM[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_attr);
            }

            break;
        
        case '\t':
            vga_col = (vga_col + 8) & ~7u;
            if (vga_col >= VGA_WIDTH) {
                vga_col = 0;
                if (++vga_row == VGA_HEIGHT) scroll();
            }
            
            break;

        default:
            VGA_MEM[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, vga_attr);
            if (++vga_col == VGA_WIDTH) {
                vga_col = 0;
                if (++vga_col == VGA_HEIGHT) scroll();
            }

            break;
    }

    update_hw_cursor();
}

void vga_print(const char* str) {
    while (*str) vga_putchar(*str++);
}

void vga_println(const char* str) {
    vga_print(str);
    vga_putchar('\n');
}

void vga_print_hex(uint32_t value) {
    const char hex[] = "0123456789ABCDEF";
    char buf[11] = "0x00000000";
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[value & 0xF];
        value >>= 4;
    }

    vga_print(buf);
}

void vga_print_uint(uint32_t value) {
    char buf[10]; 
    int i = 0;

    if (value == 0) { vga_putchar('0'); return; }
    while (value > 0) { buf[i++] = '0' + (value % 10); value /= 10; }
    while (i--) vga_putchar(buf[i]);
}

 
void vga_print_int(int32_t value) {
    if (value == 0) { vga_putchar('0'); return; }
    if (value < 0)  { vga_putchar('-'); value = -value; }
 
    char buf[12]; 
    int i = 0;

    while (value > 0) { buf[i++] = '0' + (value % 10); value /= 10; }
    while (i--) vga_putchar(buf[i]);
}

void vga_printf(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') { vga_putchar(*fmt++); continue; }
        fmt++;

        switch (*fmt++) {
            case 's': vga_print(__builtin_va_arg(ap, const char *)); break;
            case 'c': vga_putchar((char)__builtin_va_arg(ap, int));  break;
            case 'd': vga_print_int(__builtin_va_arg(ap, int32_t));  break;
            case 'u': vga_print_uint(__builtin_va_arg(ap, uint32_t)); break;
            case 'x': vga_print_hex(__builtin_va_arg(ap, uint32_t)); break;
            case '%': vga_putchar('%'); break;
            default:  vga_putchar('?'); break;
        }
    }
}

static void print_timestamp(void) {
    uint8_t d, m, s, min, h; uint16_t y;
    get_date(&d, &m, &y);
    get_time(&h, &min, &s);
 
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_putchar('[');

    vga_print_uint(d);   vga_putchar('/');
    vga_print_uint(m);   vga_putchar('/');
    vga_print_uint(y);   vga_putchar(' ');
    vga_print_uint(h);   vga_putchar(':');
    vga_print_uint(min); vga_putchar(':');
    vga_print_uint(s);

    vga_putchar(']');
}

void vga_info(const char* str) {
    uint8_t d, m, s, min, h;
    uint16_t y;

    get_date(&d, &m, &y);
    get_time(&h, &min, &s);

    vga_set_color(VGA_WHITE, VGA_BLACK);

    vga_putchar('[');
    vga_print_uint(d); vga_putchar('/');
    vga_print_uint(m); vga_putchar('/');
    vga_print_uint(y); vga_putchar(' ');

    vga_print_uint(h); vga_putchar(':');
    vga_print_uint(min); vga_putchar(':');
    vga_print_uint(s);
    vga_putchar(']');

    vga_print(" [INFO] ");
    vga_println(str);
}

void vga_error(const char* str) {
    uint8_t d, m, s, min, h;
    uint16_t y;

    get_date(&d, &m, &y);
    get_time(&h, &min, &s);

    vga_set_color(VGA_WHITE, VGA_BLACK);

    vga_putchar('[');
    vga_print_uint(d); vga_putchar('/');
    vga_print_uint(m); vga_putchar('/');
    vga_print_uint(y); vga_putchar(' ');

    vga_print_uint(h); vga_putchar(':');
    vga_print_uint(min); vga_putchar(':');
    vga_print_uint(s);
    vga_putchar(']');

    vga_set_color(VGA_RED, VGA_BLACK);
    vga_print(" [ERROR] ");
    vga_println(str);

    vga_set_color(VGA_WHITE, VGA_BLACK);
}

void vga_success(const char* str) {
    uint8_t d, m, s, min, h;
    uint16_t y;

    get_date(&d, &m, &y);
    get_time(&h, &min, &s);

    vga_set_color(VGA_WHITE, VGA_BLACK);

    vga_putchar('[');
    vga_print_uint(d); vga_putchar('/');
    vga_print_uint(m); vga_putchar('/');
    vga_print_uint(y); vga_putchar(' ');

    vga_print_uint(h); vga_putchar(':');
    vga_print_uint(min); vga_putchar(':');
    vga_print_uint(s);
    vga_putchar(']');

    vga_set_color(VGA_GREEN, VGA_BLACK);
    vga_print(" [OK] ");
    vga_println(str);

    vga_set_color(VGA_WHITE, VGA_BLACK);
}

void vga_warn(const char* str) {
    uint8_t d, m, s, min, h;
    uint16_t y;

    get_date(&d, &m, &y);
    get_time(&h, &min, &s);

    vga_set_color(VGA_WHITE, VGA_BLACK);

    vga_putchar('[');
    vga_print_uint(d); vga_putchar('/');
    vga_print_uint(m); vga_putchar('/');
    vga_print_uint(y); vga_putchar(' ');

    vga_print_uint(h); vga_putchar(':');
    vga_print_uint(min); vga_putchar(':');
    vga_print_uint(s);
    vga_putchar(']');

    vga_set_color(VGA_LIGHT_MAGENTA, VGA_BLACK);
    vga_print(" [WARNING] ");
    vga_println(str);

    vga_set_color(VGA_WHITE, VGA_BLACK);
}

// graphics
void gfx_put_pixel(uint32_t x, uint32_t y, vga_color_t color) {
    uint32_t* fb = (uint32_t*)framebuffer;
    fb[y * (pitch / 4) + x] = (uint32_t)color;
}

void gfx_clear(vga_color_t color) {
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            gfx_put_pixel(x, y, color);
        }
    }
}

void gfx_draw_line(vec2 a, vec2 b, vga_color_t color) {
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

void gfx_draw_rec(rec r, vga_color_t color) {
    // top, bottom, left, right
    for (int32_t x = r.x; x < r.x + (int32_t)r.w; x++) { gfx_put_pixel(x, r.y, color); }
    for (int32_t x = r.x; x < r.x + (int32_t)r.w; x++) { gfx_put_pixel(x, r.y + (int32_t)r.h - 1, color); }
    for (int32_t y = r.y; y < r.y + (int32_t)r.h; y++) { gfx_put_pixel(r.x, y, color); }
    for (int32_t y = r.y; y < r.y + (int32_t)r.h; y++) { gfx_put_pixel(r.x + (int32_t)r.w - 1, y, color); }
}

void gfx_draw_fill_rec(rec r, vga_color_t color) {
    for (int32_t y = r.y; y < r.y + (int32_t)r.h; y++) {
        for (int32_t x = r.x; x < r.x + (int32_t)r.w; x++) {
            gfx_put_pixel(x, y, color);
        }
    }
}

void gfx_render_frame() {
    gfx_draw_fill_rec((rec){50, 50, 50, 50}, VGA_RED);
}