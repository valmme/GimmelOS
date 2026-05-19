#include "vga.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEM ((uint16_t *)0xB8000)

static size_t vga_row = 0;
static size_t vga_col = 0;
static uint8_t vga_attr = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static uint16_t vga_entry(char c, uint8_t attr) {
    return (uint16_t)c | ((uint16_t)attr << 8);
}

static void update_hw_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_WIDTH + vga_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)(pos >> 8) & 0xFF);
}

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
 
void vga_print_int(int32_t value) {
    if (value < 0) vga_putchar('-'); value = -value;
    if (value == 0) vga_putchar('0'); return;

    char buf[12]; int i = 0;

    while (value > 0) {
        buf[i++] = '0' + (value % 10); 
        value /= 10;
    }
    
    while (i-- > 0) vga_putchar(buf[i]);
}
 
void vga_set_cursor(size_t row, size_t col) {
    vga_row = row; vga_col = col;
    update_hw_cursor();
}

static uint8_t cmos_read(uint8_t reg) {
    __asm__ volatile ("outb %1, %0" : : "dN"(0x70), "a"(reg));
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "dN"(0x71));
    return value;
}

uint8_t bcd_to_bin(uint8_t v) {
    return (v & 0x0F) + ((v >> 4) * 10);
}

uint16_t read_year() {
    uint8_t year = bcd_to_bin(cmos_read(0x09));

    if (year < 70) return 2000 + year;
    return 1900 + year;
}

void get_date(uint8_t* d, uint8_t* m, uint16_t* y) {
    *d = bcd_to_bin(cmos_read(0x07));
    *m = bcd_to_bin(cmos_read(0x08));
    *y = read_year();
}

void get_time(uint8_t* h, uint8_t* m, uint8_t* s) {
    *s = bcd_to_bin(cmos_read(0x00));
    *m = bcd_to_bin(cmos_read(0x02));
    *h = bcd_to_bin(cmos_read(0x04));
}

void vga_print_uint(uint32_t v) {
    char buf[10];
    int i = 0;

    if (v == 0) {
        vga_putchar('0');
        return;
    }

    while (v > 0) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }

    while (i--) vga_putchar(buf[i]);
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

    vga_set_color(VGA_GREEN, VGA_BLACK);
    vga_print(" [OK] ");
    vga_println(str);

    vga_set_color(VGA_WHITE, VGA_BLACK);
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