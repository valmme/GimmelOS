#include "drivers/vga/vga.h"
#include "drivers/keyboard/keyboard.h"
#include "filesystem/fs.h"
#include "lib/kstring.h"
#include "editor.h"

#define EDITOR_BUF 4096
#define EDITOR_NAME "Lito"
#define HEADER_ROWS 3
#define VISIBLE_ROWS (VGA_HEIGHT - HEADER_ROWS)

static char buf[EDITOR_BUF];
static int len = 0;
static int cursor_x = 0;
static int scroll_top = 0;
static int preferred_col = 0;
static int render_line = 0;
static char prev_word[64];
static int emit_col = 0;

static const char* keywords[] = {
    "if", "else", "for", "while", "return", "static", "const", "struct", "typedef", "enum",
    "unsigned", "signed", "switch", "case", "default", "break", "continue",
    "volatile", "extern", "inline", "sizeof", "class", "public", "private", "protected", "virtual",
    "override", "namespace", "using", "this", "try", "except", "throw", "catch"
};

vga_color_t FUNCTIONS_C = VGA_LIGHT_BROWN;
vga_color_t KEYWORDS_C = VGA_LIGHT_MAGENTA;
vga_color_t TYPES_C = VGA_LIGHT_CYAN;
vga_color_t MACROS_C = VGA_LIGHT_RED;
vga_color_t STRINGS_C = VGA_LIGHT_GREEN;
vga_color_t NUMBERS_C = VGA_CYAN;
vga_color_t CLASSES_C = VGA_WHITE;
vga_color_t COMMENTS_C = VGA_DARK_GREY;

static int ends_with(const char* str, const char* ext) {
    int slen = kstrlen(str);
    int elen = kstrlen(ext);
    if (slen < elen) return 0;
    return kstrcmp(str + slen - elen, ext) == 0;
}

static int is_c_file(const char* filename) {
    return ends_with(filename, ".c") ||
           ends_with(filename, ".h") ||
           ends_with(filename, ".cc") ||
           ends_with(filename, ".cpp") ||
           ends_with(filename, ".hpp");
}

static void insert_char(char c) {
    if (len >= EDITOR_BUF - 1) return;
    for (int i = len; i > cursor_x; i--) buf[i] = buf[i - 1];
    buf[cursor_x] = c;
    len++;
    cursor_x++;
    buf[len] = 0;
}

static void delete_char(void) {
    if (cursor_x <= 0) return;
    for (int i = cursor_x - 1; i < len; i++) buf[i] = buf[i + 1];
    cursor_x--;
    len--;
}

static int find_line_start(int target_line) {
    if (target_line == 0) return 0;
    int pos = 0, line = 0;
    while (pos < len) {
        if (buf[pos] == '\n') {
            line++;
            if (line == target_line) return pos + 1;
        }
        pos++;
    }
    return pos;
}

static void get_line_col(int* line_out, int* col_out) {
    int line = 0, col = 0;
    for (int i = 0; i < cursor_x; i++) {
        if (buf[i] == '\n') { line++; col = 0; }
        else col++;
    }
    *line_out = line;
    *col_out = col;
}

static int line_length(int start) {
    int l = 0;
    while (start + l < len && buf[start + l] != '\n') l++;
    return l;
}

static void update_scroll(void) {
    int line, col;
    get_line_col(&line, &col);
    
    if (line < scroll_top) scroll_top = line;
    else if (line >= scroll_top + VISIBLE_ROWS) scroll_top = line - VISIBLE_ROWS + 1;
}

static void get_visual_pos(int* vrow_out, int* vcol_out) {
    int line, col;
    get_line_col(&line, &col);

    int vrow = 0;
    for (int l = scroll_top; l < line; l++) {
        int start = find_line_start(l);
        int llen = line_length(start);
        int rows = llen / VGA_WIDTH;
        
        if (llen % VGA_WIDTH != 0 || llen == 0) rows++;
        vrow += rows;
    }

    vrow += col / VGA_WIDTH;
    *vrow_out = vrow;
    *vcol_out = col % VGA_WIDTH;
}

static int is_keyword(const char* word) {
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
        if (kstrcmp(word, keywords[i]) == 0) return 1;
    return 0;
}

static int is_type_starter(const char* word) {
    return kstrcmp(word, "struct") == 0 ||
           kstrcmp(word, "class") == 0 ||
           kstrcmp(word, "enum") == 0 ||
           kstrcmp(word, "typedef") == 0;
}

static void emit(char c) {
    if (render_line >= scroll_top && render_line < scroll_top + VISIBLE_ROWS) {
        int screen_row = (render_line - scroll_top) + HEADER_ROWS;
        if (c != '\n') {
            vga_put_at(c, screen_row, emit_col);
        }
    }

    if (c == '\n') {
        render_line++;
        emit_col = 0;
    } 
    
    else {
        emit_col++;
        if (emit_col >= VGA_WIDTH) {
            emit_col = 0;
            render_line++;
        }
    }
}

static void emit_color(vga_color_t fg, vga_color_t bg) {
    if (render_line >= scroll_top && render_line < scroll_top + VISIBLE_ROWS)
        vga_set_color(fg, bg);
}

static void draw_with_syntax(void) {
    char word[64];
    int w = 0;
    int in_string = 0;
    int in_macro = 0;
    render_line = 0;
    emit_col = 0;
    prev_word[0] = '\0';

    for (int i = 0; i < len; i++) {
        char c = buf[i];

        if (!in_string && c == '/' && i + 1 < len && buf[i + 1] == '/') {
            if (w > 0) {
                word[w] = 0;
                emit_color(is_keyword(word) ? KEYWORDS_C : VGA_WHITE, VGA_BLACK);
                for (int j = 0; j < w; j++) emit(word[j]);
                w = 0;
            }
            emit_color(COMMENTS_C, VGA_BLACK);
            while (i < len && buf[i] != '\n') { emit(buf[i]); i++; }
            if (i < len && buf[i] == '\n') emit('\n');
            continue;
        }

        if (c == '"' && !in_macro) {
            if (w > 0) {
                word[w] = 0;
                emit_color(is_keyword(word) ? KEYWORDS_C : VGA_WHITE, VGA_BLACK);
                for (int j = 0; j < w; j++) emit(word[j]);
                w = 0;
            }
            in_string = !in_string;
            emit_color(STRINGS_C, VGA_BLACK);
            emit(c);
            continue;
        }

        if (in_string) {
            emit_color(STRINGS_C, VGA_BLACK);
            emit(c);
            continue;
        }

        if (c == '#') {
            if (w > 0) {
                word[w] = 0;
                emit_color(is_keyword(word) ? KEYWORDS_C : VGA_WHITE, VGA_BLACK);
                for (int j = 0; j < w; j++) emit(word[j]);
                w = 0;
            }
            in_macro = 1;
        }

        if (in_macro) {
            emit_color(MACROS_C, VGA_BLACK);
            emit(c);
            if (c == ' ' || c == '\n') in_macro = 0;
            continue;
        }

        if (c == ':' && i + 1 < len && buf[i + 1] == ':') {
            if (w > 0) {
                word[w] = 0;
                emit_color(CLASSES_C, VGA_BLACK);
                for (int j = 0; j < w; j++) emit(word[j]);
                w = 0;
            }
            emit_color(CLASSES_C, VGA_BLACK);
            emit(':'); emit(':');
            i++;
            continue;
        }

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            if (w < 63) word[w++] = c;
            continue;
        }

        if (w > 0) {
            word[w] = 0;
            int color = VGA_WHITE;

            if (is_keyword(word)) {
                color = KEYWORDS_C;
                if (is_type_starter(word)) kstrncpy(prev_word, word, 64);
                else prev_word[0] = '\0';
            } else if (prev_word[0] != '\0') {
                color = TYPES_C;
                prev_word[0] = '\0';
            } else if (c == '(') {
                color = FUNCTIONS_C;
            } else if (c == ' ') {
                int peek = i + 1;
                while (peek < len && buf[peek] == ' ') peek++;
                char nc = (peek < len) ? buf[peek] : 0;
                if ((nc >= 'a' && nc <= 'z') || (nc >= 'A' && nc <= 'Z') || nc == '_')
                    color = TYPES_C;
            }

            emit_color(color, VGA_BLACK);
            for (int j = 0; j < w; j++) emit(word[j]);
            w = 0;
        }

        if (c >= '0' && c <= '9') {
            emit_color(NUMBERS_C, VGA_BLACK);
            emit(c);
            emit_color(VGA_WHITE, VGA_BLACK);
            continue;
        }

        emit_color(VGA_WHITE, VGA_BLACK);
        emit(c);
    }

    if (w > 0) {
        word[w] = 0;
        emit_color(is_keyword(word) ? KEYWORDS_C : VGA_WHITE, VGA_BLACK);
        for (int j = 0; j < w; j++) emit(word[j]);
    }
}

static void editor_draw(const char* filename) {
    vga_clear();

    vga_set_color(VGA_BLACK, VGA_WHITE);
    vga_printf("%s - %s", EDITOR_NAME, filename);

    vga_set_cursor(1, 0);
    vga_set_color(VGA_BLACK, VGA_LIGHT_GREY);
    vga_print("Ctrl+S Save | Ctrl+Z Exit");

    vga_set_cursor(HEADER_ROWS, 0);
    vga_set_color(VGA_WHITE, VGA_BLACK);

    if (is_c_file(filename)) {
        draw_with_syntax();
    } 
    
    else {
        render_line = 0;
        emit_col = 0;
        for (int i = 0; i < len; i++) emit(buf[i]);
    }

    int vrow, vcol;
    get_visual_pos(&vrow, &vcol);
    vga_set_cursor(vrow + HEADER_ROWS, vcol);
}

void editor_open(const char* filename) {
    kmemset(buf, 0, sizeof(buf));
    fs_read(filename, 0, (uint8_t*)buf);
    buf[EDITOR_BUF - 1] = 0;
    len = kstrlen(buf);
    cursor_x = len;
    scroll_top = 0;
    update_scroll();

    while (1) {
        editor_draw(filename);

        int c = keyboard_getchar();
        if (!c) continue;

        if (c == 19) {
            fs_write(filename, 0, (uint8_t*)buf, len);
            continue;
        }

        if (c == 26) break;

        if (c == KEY_LEFT) {
            if (cursor_x > 0) cursor_x--;
            int line, col;
            get_line_col(&line, &col);
            preferred_col = col;
            update_scroll();
            continue;
        }

        if (c == KEY_RIGHT) {
            if (cursor_x < len) cursor_x++;
            int line, col;
            get_line_col(&line, &col);
            preferred_col = col;
            update_scroll();
            continue;
        }

        if (c == KEY_UP) {
            int line, col;
            get_line_col(&line, &col);
            if (line == 0) continue;
            int start = find_line_start(line - 1);
            int llen = line_length(start);
            cursor_x = start + (preferred_col < llen ? preferred_col : llen);
            update_scroll();
            continue;
        }

        if (c == KEY_DOWN) {
            int line, col;
            get_line_col(&line, &col);
            int start = find_line_start(line + 1);
            if (start >= len) continue;
            int llen = line_length(start);
            cursor_x = start + (preferred_col < llen ? preferred_col : llen);
            update_scroll();
            continue;
        }

        if (c == '\t') {
            for (int i = 0; i < 4; i++) insert_char(' ');
            update_scroll();
            continue;
        }

        if (c == '\b') {
            delete_char();
            int line, col;
            get_line_col(&line, &col);
            preferred_col = col;
            update_scroll();
            continue;
        }

        if (c == '\n') {
            insert_char('\n');
            int line, col;
            get_line_col(&line, &col);
            preferred_col = col;
            update_scroll();
            continue;
        }

        if (c >= 32 && c <= 126) {
            insert_char((char)c);
            int line, col;
            get_line_col(&line, &col);
            preferred_col = col;
            update_scroll();
        }
    }
}