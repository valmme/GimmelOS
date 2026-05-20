#include "drivers/vga/vga.h"
#include "drivers/keyboard/keyboard.h"
#include "filesystem/fs.h"
#include "lib/kstring.h"
#include "editor.h"

#define EDITOR_BUF 4096
#define EDITOR_NAME "Lito"

static char buf[EDITOR_BUF];

static int len = 0;
static int cursor_x = 0;
static int cursor_y = 0;

static void get_cursor(int pos, int* x, int* y) {
    int col = 0;
    int row = 0;

    for (int i = 0; i < pos; i++) {
        if (buf[i] == '\n') {
            row++;
            col = 0;
        }

        else {
            col++;
        }
    }

    *x = col;
    *y = row + 3;
}

static void editor_draw(const char* filename) {
    vga_clear();

    vga_set_color(VGA_BLACK, VGA_WHITE);
    vga_printf("%s - %s", EDITOR_NAME, filename);

    vga_set_cursor(1, 0);

    vga_set_color(VGA_BLACK, VGA_LIGHT_GREY);
    vga_print("Ctrl+S Save | Ctrl+Z Exit");

    vga_set_cursor(3, 0);

    vga_set_color(VGA_WHITE, VGA_BLACK);

    for (int i = 0; i < len; i++) {
        vga_putchar(buf[i]);
    }

    int row, col;
    get_cursor(cursor_x, &col, &row);
    vga_set_cursor(row, col);
}

static void insert_char(char c) {
    if (len >= EDITOR_BUF - 1)
        return;

    for (int i = len; i > cursor_x; i--) {
        buf[i] = buf[i - 1];
    }

    buf[cursor_x] = c;

    len++;
    cursor_x++;

    buf[len] = 0;
}

static void delete_char() {
    if (cursor_x <= 0)
        return;

    for (int i = cursor_x - 1; i < len; i++) {
        buf[i] = buf[i + 1];
    }

    cursor_x--;
    len--;
}

void editor_open(const char* filename) {
    kmemset(buf, 0, sizeof(buf));

    fs_read(filename, 0, (uint8_t*)buf);

    len = kstrlen(buf);
    cursor_x = len;

    while (1) {
        editor_draw(filename);

        int c = keyboard_getchar();

        if (!c)
            continue;

        if (c == 19) {
            fs_write(filename, 0, (uint8_t*)buf, len);
            continue;
        }

        if (c == 26) {
            vga_println("");
            break;
        }

        if (c == KEY_LEFT) {
            if (cursor_x > 0)
                cursor_x--;

            continue;
        }

        if (c == KEY_RIGHT) {
            if (cursor_x < len)
                cursor_x++;

            continue;
        }

        if (c == KEY_UP) {
            int x = 0, y = 0;
            get_cursor(cursor_x, &x, &y);

            if (y <= 0) continue;

            int target_y = y - 1 - 3;

            int pos = 0;
            int row = 0;

            while (pos < len && row < target_y) {
                if (buf[pos] == '\n') row++;
                pos++;
            }

            int col = x;
            cursor_x = pos + col;

            if (cursor_x > len) cursor_x = len;

            continue;
        }

        if (c == KEY_DOWN) {
            int x = 0, y = 0;
            get_cursor(cursor_x, &x, &y);

            int target_y = y - 3 + 1;

            int pos = 0;
            int row = 0;

            while (pos < len && row < target_y) {
                if (buf[pos] == '\n') row++;
                pos++;
            }

            int col = x;
            cursor_x = pos + col;

            if (cursor_x > len) cursor_x = len;

            continue;
        }

        if (c == '\b') {
            delete_char();
            continue;
        }

        if (c == '\n') {
            insert_char('\n');
            continue;
        }

        if (c >= 32 && c <= 126) {
            insert_char((char)c);
        }
    }
}