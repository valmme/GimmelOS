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

static const char* keywords[] = {
    "int", "char", "void", "if", "else", "for", "while", "return", "static", "const", "struct", "typedef", "enum",
    "float", "double", "long", "short", "unsigned", "signed", "switch", "case", "default", "break", "continue",
    "volatile", "extern", "inline", "bool", "sizeof", "string", "class", "public", "private", "protected", "virtual", 
    "override", "namespace", "using", "this", "try", "except", "throw", "catch", "uint8_t", "uint16_t", "uint32_t", "uint64_t"
};

static const char* macros[] = {
    "#include", "#define", "#undef", "#if", "#ifdef", "#ifndef", "#elif", "#else", "#endif", "#error", "#line", "#pragma"
};

static int ends_with(const char* str, const char* ext) {
    int slen = kstrlen(str);
    int elen = kstrlen(ext);

    if (slen < elen)
        return 0;

    return kstrcmp(str + slen - elen, ext) == 0;
}

static int is_c_file(const char* filename) {
    return
        ends_with(filename, ".c")   ||
        ends_with(filename, ".h")   ||
        ends_with(filename, ".cc")  ||
        ends_with(filename, ".cpp") ||
        ends_with(filename, ".hpp");
}

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

static int is_keyword(const char* word) {
    for (int i = 0; i < 9; i++) {
        if (kstrcmp(word, keywords[i]) == 0)
            return 1;
    }

    return 0;
}

static int is_macro(const char* word) {
    for (int i = 0; i < 12; i++) {
        if (kstrcmp(word, macros[i]) == 0)
            return 1;
    }

    return 0;
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

static void draw_with_syntax() {
    char word[64];
    int w = 0;
    int in_string = 0;
    int in_macro = 0;

    for (int i = 0; i < len; i++) {
        char c = buf[i];

        if (!in_string && c == '/' && i + 1 < len && buf[i + 1] == '/') {
            if (w > 0) {
                word[w] = 0;
                int color = is_keyword(word) ? VGA_LIGHT_BLUE : VGA_WHITE;
                vga_set_color(color, VGA_BLACK);

                for (int j = 0; j < w; j++) vga_putchar(word[j]);
                w = 0;
            }

            vga_set_color(VGA_DARK_GREY, VGA_BLACK);
            while (i < len && buf[i] != '\n') {
                vga_putchar(buf[i]);
                i++;
            }

            if (i < len && buf[i] == '\n') vga_putchar('\n');
            continue;
        }

        if (c == '"' && !in_macro) {
            if (w > 0) {
                word[w] = 0;
                int color = is_keyword(word) ? VGA_LIGHT_BLUE : VGA_WHITE;
                vga_set_color(color, VGA_BLACK);

                for (int j = 0; j < w; j++) vga_putchar(word[j]);
                w = 0;
            }

            in_string = !in_string;
            vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
            vga_putchar(c);
            continue;
        }

        if (in_string) {
            vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
            vga_putchar(c);
            continue;
        }

        if (c == '#') {
            if (w > 0) {
                word[w] = 0;
                int color = is_keyword(word) ? VGA_LIGHT_BLUE : VGA_WHITE;

                vga_set_color(color, VGA_BLACK);

                for (int j = 0; j < w; j++) vga_putchar(word[j]);
                w = 0;
            }

            in_macro = 1;
        }

        if (in_macro) {
            vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
            vga_putchar(c);

            if (c == ' ' || c == '\n') in_macro = 0;
            continue;
        }

        if (c == ':' && i + 1 < len && buf[i + 1] == ':') {
            if (w > 0) {
                word[w] = 0;
                vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);

                for (int j = 0; j < w; j++) vga_putchar(word[j]);
                w = 0;
            }

            vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
            vga_putchar(':');
            vga_putchar(':');

            i++;
            continue;
        }

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            if (w < 63) word[w++] = c;
            continue;
        }

        if (w > 0) {
            word[w] = 0;
            int is_kw = is_keyword(word);
            int is_func = (c == '(');
            int color = VGA_WHITE;

            if (is_kw) color = VGA_LIGHT_BLUE;
            else if (is_func) color = VGA_LIGHT_BROWN;

            vga_set_color(color, VGA_BLACK);

            for (int j = 0; j < w; j++) vga_putchar(word[j]);
            w = 0;
        }

        if (c >= '0' && c <= '9') {
            vga_set_color(VGA_CYAN, VGA_BLACK);
            vga_putchar(c);
            vga_set_color(VGA_WHITE, VGA_BLACK);

            continue;
        }

        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_putchar(c);
    }

    if (w > 0) {
        word[w] = 0;
        int color = is_keyword(word) ? VGA_LIGHT_BLUE : VGA_WHITE;
        vga_set_color(color, VGA_BLACK);

        for (int j = 0; j < w; j++) vga_putchar(word[j]);
    }
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

    if (is_c_file(filename)) draw_with_syntax();
    else vga_print(buf);

    int row, col;
    get_cursor(cursor_x, &col, &row);
    vga_set_cursor(row, col);
}

void editor_open(const char* filename) {
    kmemset(buf, 0, sizeof(buf));

    fs_read(filename, 0, (uint8_t*)buf);
    buf[EDITOR_BUF - 1] = 0;
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
            int lines = 1;

            for (int i = 0; i < len; i++) {
                if (buf[i] == '\n') lines++;
            }

            vga_set_cursor(lines + 3, 0);
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

        if (c == '\t') {
            for (int i = 0; i < 4; i++) {
                insert_char(' ');
            }
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