#include "gfx/ui/wm.h"
#include "drivers/keyboard/keyboard.h"
#include "filesystem/fs.h"
#include "lib/kstring.h"
#include "editor.h"

#define CHAR_W 8
#define CHAR_H 8
#define SCALE 1

#define TOOLBAR_H 20
#define STATUSBAR_H 18
#define HEADER_H TOOLBAR_H

#define EDITOR_PADDING_X 6
#define EDITOR_PADDING_Y 6
#define EDITOR_LINE_SPACING 2
#define EDITOR_LINE_HEIGHT (CHAR_H * SCALE + EDITOR_LINE_SPACING)

#define EDITOR_BUF 4096
#define EDITOR_NAME "Lito"

#define C_BG           (gfx_color_t){30, 30, 30, 255}
#define C_TOOLBAR      (gfx_color_t){60, 60, 60, 255}
#define C_STATUSBAR    (gfx_color_t){45, 45, 45, 255}
#define C_CURSOR       GFX_WHITE
#define C_CURSOR_LINE  (gfx_color_t){50, 45, 40, 255}
#define C_TEXT         GFX_WHITE
#define C_LINENUM      GFX_GRAY
#define C_SCROLLBAR_BG GFX_DARK_GRAY
#define C_SCROLLBAR_FG GFX_GRAY
#define C_KEYWORD      (gfx_color_t){198, 120, 221, 255}
#define C_TYPE         (gfx_color_t){97 , 175, 239, 255}
#define C_FUNCTION     (gfx_color_t){229, 192, 123, 255}
#define C_STRING       (gfx_color_t){152, 195, 121, 255}
#define C_NUMBER       (gfx_color_t){209, 154, 102, 255}
#define C_MACRO        (gfx_color_t){224, 108, 117, 255}
#define C_COMMENT      (gfx_color_t){92 , 99 , 112, 255}
#define C_CLASS        (gfx_color_t){86 , 182, 194, 255}
#define C_PUNCT        (gfx_color_t){171, 178, 191, 255}

static char buf[EDITOR_BUF];
static int len = 0;
static int cursor_x = 0;
static int scroll_top = 0;
static int preferred_col = 0;
static char current_filename[128];

static const char *keywords[] = {
    "if", "else", "for", "while", "return", "static", "const", "struct",
    "typedef", "enum", "unsigned", "signed", "switch", "case", "default",
    "break", "continue", "volatile", "extern", "inline", "sizeof",
    "class", "public", "private", "protected", "virtual",
    "override", "namespace", "using", "this", "try", "except", "throw", "catch",
    "int", "char", "void", "float", "double", "long", "short", "uint8_t",
    "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t",
    "int64_t", "size_t", "NULL", "nullptr", "true", "false", "bool"
};

static int ends_with(const char *str, const char *ext) {
    int slen = kstrlen(str);
    int elen = kstrlen(ext);
    if (slen < elen) return 0;
    return kstrcmp(str + slen - elen, ext) == 0;
}

static int is_c_file(const char *filename) {
    return ends_with(filename, ".c") || ends_with(filename, ".h") ||
           ends_with(filename, ".cc") || ends_with(filename, ".cpp") ||
           ends_with(filename, ".hpp");
}

static int is_keyword(const char *word) {
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
        if (kstrcmp(word, keywords[i]) == 0) return 1;
    return 0;
}

static int is_type_starter(const char *word) {
    return kstrcmp(word, "struct") == 0 || kstrcmp(word, "class") == 0 ||
           kstrcmp(word, "enum") == 0 || kstrcmp(word, "typedef") == 0;
}

static void insert_char(char c) {
    if (len >= EDITOR_BUF - 1) return;
    for (int i = len; i > cursor_x; i--) buf[i] = buf[i - 1];
    buf[cursor_x++] = c;
    len++;
    buf[len] = 0;
}

static void delete_char(void) {
    if (cursor_x <= 0) return;
    for (int i = cursor_x - 1; i < len; i++) buf[i] = buf[i + 1];
    cursor_x--;
    len--;
}

static void get_line_col(int *line_out, int *col_out) {
    int line = 0, col = 0;
    for (int i = 0; i < cursor_x; i++) {
        if (buf[i] == '\n') { line++; col = 0; }
        else col++;
    }
    *line_out = line;
    *col_out = col;
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

static int line_length(int start) {
    int l = 0;
    while (start + l < len && buf[start + l] != '\n') l++;
    return l;
}

static int count_total_lines(void) {
    int lines = 1;
    for (int i = 0; i < len; i++)
        if (buf[i] == '\n') lines++;
    return lines;
}

static void update_scroll(int visible_rows) {
    int line, col;
    get_line_col(&line, &col);
    if (line < scroll_top) scroll_top = line;
    else if (line >= scroll_top + visible_rows) scroll_top = line - visible_rows + 1;
}

static int digits(int n) {
    int d = 1;
    while (n >= 10) { n /= 10; d++; }
    return d;
}

static void draw_int(int n, vec2 pos, gfx_color_t fg, gfx_color_t bg) {
    char tmp[12];
    int i = 0;
    if (n == 0) { tmp[i++] = '0'; }
    else {
        int rev = 0, cnt = 0, x = n;
        while (x > 0) { rev = rev * 10 + x % 10; x /= 10; cnt++; }
        for (int j = 0; j < cnt; j++) { tmp[i++] = '0' + (rev % 10); rev /= 10; }
    }
    tmp[i] = 0;
    wm_draw_text(tmp, pos, fg, bg);
}

typedef struct {
    int canvas_w;
    int visible_rows;
    int text_x0;
    int cur_line;
    int cur_col;
    gfx_color_t cur_color;
} render_ctx_t;

static void rc_newline(render_ctx_t *rc) {
    rc->cur_line++;
    rc->cur_col = 0;
}

static void rc_emit(render_ctx_t *rc, char c) {
    int rel = rc->cur_line - scroll_top;
    if (rel >= 0 && rel < rc->visible_rows) {
        int px = rc->text_x0 + rc->cur_col * CHAR_W * SCALE;
        int py = HEADER_H + EDITOR_PADDING_Y + rel * EDITOR_LINE_HEIGHT;
        if (px >= rc->text_x0 && px + CHAR_W * SCALE <= rc->canvas_w)
            wm_putchar_ex(c, (vec2){px, py}, rc->cur_color, C_BG, SCALE);
    }
    rc->cur_col++;
}

static void rc_emit_str(render_ctx_t *rc, const char *s, int n) {
    for (int i = 0; i < n; i++) rc_emit(rc, s[i]);
}

static void draw_plain(render_ctx_t *rc) {
    for (int i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '\n') rc_newline(rc);
        else { rc->cur_color = C_TEXT; rc_emit(rc, c); }
    }
}

static void draw_syntax(render_ctx_t *rc) {
    char word[64];
    int w = 0, in_string = 0, in_macro = 0;
    char prev_word[64];
    prev_word[0] = '\0';

    for (int i = 0; i <= len; i++) {
        char c = (i < len) ? buf[i] : 0;

        if (!in_string && c == '/' && i + 1 < len && buf[i + 1] == '/') {
            if (w > 0) {
                word[w] = 0;
                rc->cur_color = is_keyword(word) ? C_KEYWORD : C_TEXT;
                rc_emit_str(rc, word, w);
                w = 0;
            }
            rc->cur_color = C_COMMENT;
            while (i < len && buf[i] != '\n') { rc_emit(rc, buf[i]); i++; }
            if (i < len) rc_newline(rc);
            continue;
        }

        if (c == '"' && !in_macro) {
            if (w > 0) {
                word[w] = 0;
                rc->cur_color = is_keyword(word) ? C_KEYWORD : C_TEXT;
                rc_emit_str(rc, word, w);
                w = 0;
            }

            in_string = !in_string;
            rc->cur_color = C_STRING;
            rc_emit(rc, c);
            continue;
        }

        if (in_string) {
            rc->cur_color = C_STRING;
            if (c == '\n') rc_newline(rc);
            else rc_emit(rc, c);
            continue;
        }

        if (c == '#') {
            if (w > 0) {
                word[w] = 0;
                rc->cur_color = is_keyword(word) ? C_KEYWORD : C_TEXT;
                rc_emit_str(rc, word, w);
                w = 0;
            }

            in_macro = 1;
        }
        if (in_macro) {
            rc->cur_color = C_MACRO;
            if (c == '\n') { rc_newline(rc); in_macro = 0; }
            else rc_emit(rc, c);
            continue;
        }

        if (c == ':' && i + 1 < len && buf[i + 1] == ':') {
            if (w > 0) {
                word[w] = 0;
                rc->cur_color = C_CLASS;
                rc_emit_str(rc, word, w);
                w = 0;
            }

            rc->cur_color = C_CLASS;
            rc_emit(rc, ':'); rc_emit(rc, ':');
            i++;
            continue;
        }

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
            (w > 0 && c >= '0' && c <= '9')) {
            if (w < 63) word[w++] = c;
            continue;
        }

        if (w > 0) {
            word[w] = 0;
            gfx_color_t color = C_TEXT;
            if (is_keyword(word)) {
                color = C_KEYWORD;
                if (is_type_starter(word)) kstrncpy(prev_word, word, 64);
                else prev_word[0] = '\0';
            } 
            
            else if (prev_word[0] != '\0') {
                color = C_TYPE;
                prev_word[0] = '\0';
            }
            
            else if (c == '(') {
                color = C_FUNCTION;
            } 
            
            else if (c == ' ' || c == '\t') {
                int peek = i + 1;
                while (peek < len && (buf[peek] == ' ' || buf[peek] == '\t')) peek++;
                char nc = (peek < len) ? buf[peek] : 0;

                if ((nc >= 'a' && nc <= 'z') || (nc >= 'A' && nc <= 'Z') || nc == '_')
                    color = C_TYPE;
            }

            rc->cur_color = color;
            rc_emit_str(rc, word, w);
            w = 0;
        }

        if (c == 0) break;
        if (c == '\n') { rc_newline(rc); continue; }

        if (c >= '0' && c <= '9') {
            rc->cur_color = C_NUMBER;
            rc_emit(rc, c);
            continue;
        }

        if (c == '(' || c == ')' || c == '{' || c == '}' ||
            c == '[' || c == ']' || c == ';' || c == ',' ||
            c == '.' || c == '*' || c == '&' || c == '|' ||
            c == '!' || c == '=' || c == '<' || c == '>' ||
            c == '+' || c == '-' || c == '/' || c == '%' ||
            c == '^' || c == '~' || c == '?' || c == ':')
            rc->cur_color = C_PUNCT;
        else
            rc->cur_color = C_TEXT;
        rc_emit(rc, c);
    }
}

void editor_draw(int wid) {
    wm_canvas_t canvas = wm_get_canvas(wid);
    int cw = canvas.w;
    int ch = canvas.h;

    int total_lines = count_total_lines();
    int linenum_w = (digits(total_lines) + 1) * CHAR_W * SCALE + EDITOR_PADDING_X * 2;

    int visible_rows = (ch - HEADER_H - STATUSBAR_H) / (EDITOR_LINE_HEIGHT + EDITOR_LINE_SPACING);
    if (visible_rows < 1) visible_rows = 1;

    int cursor_line, cursor_col;
    get_line_col(&cursor_line, &cursor_col);

    wm_begin_draw(wid);

    wm_draw_fill_rec((rec){0, 0, cw, ch}, C_BG);
    wm_draw_fill_rec((rec){0, 0, cw, TOOLBAR_H}, C_TOOLBAR);

    {
        int ty = (TOOLBAR_H - EDITOR_LINE_HEIGHT) / 2;
        wm_draw_text_ex(EDITOR_NAME "  |  ^S Save", (vec2){EDITOR_PADDING_X, ty}, C_TEXT, C_TOOLBAR, SCALE);

        const char *lang = is_c_file(current_filename) ? "C/C++" : "Plain";
        int lw = kstrlen(lang) * CHAR_W * SCALE;
        wm_draw_text_ex(lang, (vec2){cw - lw - EDITOR_PADDING_X, ty}, C_LINENUM, C_TOOLBAR, SCALE);
    }

    wm_draw_fill_rec((rec){0, ch - STATUSBAR_H, cw, STATUSBAR_H}, C_STATUSBAR);

    {
        int sy = ch - STATUSBAR_H + (STATUSBAR_H - EDITOR_LINE_HEIGHT) / 2;
        char lc[32];
        int li = 0;
        lc[li++] = 'L'; lc[li++] = 'n'; lc[li++] = ' ';

        { char rev[12]; int ri = 0, x = cursor_line + 1; while (x > 0) { rev[ri++] = '0' + x % 10; x /= 10; } for (int k = ri - 1; k >= 0; k--) lc[li++] = rev[k]; }
        lc[li++] = ' '; lc[li++] = ' ';
        lc[li++] = 'C'; lc[li++] = 'o'; lc[li++] = 'l'; lc[li++] = ' ';

        { char rev[12]; int ri = 0, x = cursor_col + 1; while (x > 0) { rev[ri++] = '0' + x % 10; x /= 10; } for (int k = ri - 1; k >= 0; k--) lc[li++] = rev[k]; }
        lc[li] = 0;
        int lcw = kstrlen(lc) * CHAR_W * SCALE;

        wm_draw_text_ex(lc, (vec2){cw - lcw - EDITOR_PADDING_X, sy}, C_LINENUM, C_STATUSBAR, SCALE);
        wm_draw_text_ex(current_filename, (vec2){EDITOR_PADDING_X, sy}, C_TEXT, C_STATUSBAR, SCALE);
    }

    int sb_x = cw - 6;
    int sb_y0 = HEADER_H + 1;
    int sb_h = ch - HEADER_H - STATUSBAR_H - 1;

    wm_draw_fill_rec((rec){sb_x, sb_y0, 6, sb_h}, C_SCROLLBAR_BG);
    if (total_lines > visible_rows) {
        int thumb_h = sb_h * visible_rows / total_lines;
        if (thumb_h < 8) thumb_h = 8;
        int thumb_y = sb_y0 + (sb_h - thumb_h) * scroll_top / (total_lines - visible_rows);
        wm_draw_fill_rec((rec){sb_x + 1, thumb_y, 4, thumb_h}, C_SCROLLBAR_FG);
    }

    wm_draw_fill_rec((rec){0, HEADER_H + 1, linenum_w, sb_h}, C_TOOLBAR);
    wm_draw_line((vec2){linenum_w, HEADER_H + 1}, (vec2){linenum_w, ch - STATUSBAR_H}, C_SCROLLBAR_BG);

    {
        int rel = cursor_line - scroll_top;
        if (rel >= 0 && rel < visible_rows) {
            int hy = HEADER_H + EDITOR_PADDING_Y + rel * EDITOR_LINE_HEIGHT;
            wm_draw_fill_rec((rec){linenum_w + 1, hy, sb_x - linenum_w - 1, EDITOR_LINE_HEIGHT}, C_CURSOR_LINE);
        }
    }

    for (int r = 0; r < visible_rows; r++) {
        int abs_line = scroll_top + r;
        if (abs_line >= total_lines) break;
        int py = HEADER_H + EDITOR_PADDING_Y + r * EDITOR_LINE_HEIGHT;
        gfx_color_t fg = (abs_line == cursor_line) ? C_TEXT : C_LINENUM;
        int num_digits = digits(abs_line + 1);
        int px = linenum_w - EDITOR_PADDING_X - num_digits * CHAR_W * SCALE;
        draw_int(abs_line + 1, (vec2){px, py}, fg, C_TOOLBAR);
    }

    {
        render_ctx_t rc;
        rc.canvas_w = sb_x;
        rc.visible_rows = visible_rows;
        rc.text_x0 = linenum_w + EDITOR_PADDING_X + 1;
        rc.cur_line = 0;
        rc.cur_col = 0;
        rc.cur_color = C_TEXT;
        if (!is_c_file(current_filename)) draw_syntax(&rc);
        else draw_plain(&rc);
    }

    {
        int rel = cursor_line - scroll_top;
        if (rel >= 0 && rel < visible_rows) {
            int cx = linenum_w + EDITOR_PADDING_X + 1 + cursor_col * CHAR_W * SCALE;
            int cy = HEADER_H + EDITOR_PADDING_Y + rel * EDITOR_LINE_HEIGHT;
            wm_draw_fill_rec((rec){cx, cy, 2, EDITOR_LINE_HEIGHT}, C_CURSOR);
        }
    }

    wm_end_draw();
}

static int get_visible_rows(int wid) {
    wm_canvas_t canvas = wm_get_canvas(wid);
    int rows = (canvas.h - HEADER_H - STATUSBAR_H) / (EDITOR_LINE_HEIGHT);
    return rows < 1 ? 1 : rows;
}

void editor_init(int wid) {
    kmemset(buf, 0, EDITOR_BUF);
    fs_read(current_filename, 0, (uint8_t *)buf);
    buf[EDITOR_BUF - 1] = 0;
    len = kstrlen(buf);
    cursor_x = 0;
    scroll_top = 0;
    preferred_col = 0;
    update_scroll(get_visible_rows(wid));
}

void editor_update(int wid) {
    int visible_rows = get_visible_rows(wid);
    int c;

    while ((c = keyboard_getchar_nonblocking())) {
        if (c) {
            if (c == 19) {
                fs_write(current_filename, 0, (uint8_t *)buf, len);
            } 
            
            else if (c == KEY_LEFT) {
                if (cursor_x > 0) cursor_x--;
                int line, col; get_line_col(&line, &col);
                preferred_col = col;
                update_scroll(visible_rows);
            } 
            
            else if (c == KEY_RIGHT) {
                if (cursor_x < len) cursor_x++;
                int line, col; get_line_col(&line, &col);
                preferred_col = col;
                update_scroll(visible_rows);
            } 
            
            else if (c == KEY_UP) {
                int line, col; get_line_col(&line, &col);
                if (line > 0) {
                    int start = find_line_start(line - 1);
                    int llen = line_length(start);
                    cursor_x = start + (preferred_col < llen ? preferred_col : llen);
                    update_scroll(visible_rows);
                }
            } 
            
            else if (c == KEY_DOWN) {
                int line, col; get_line_col(&line, &col);
                int start = find_line_start(line + 1);
                if (start < len) {
                    int llen = line_length(start);
                    cursor_x = start + (preferred_col < llen ? preferred_col : llen);
                    update_scroll(visible_rows);
                }
            } 
            
            else if (c == '\t') {
                for (int i = 0; i < 4; i++) insert_char(' ');
                int line, col; get_line_col(&line, &col);
                preferred_col = col;
                update_scroll(visible_rows);
            } 
            
            else if (c == '\b') {
                delete_char();
                int line, col; get_line_col(&line, &col);
                preferred_col = col;
                update_scroll(visible_rows);
            } 
            
            else if (c == '\n') {
                insert_char('\n');
                int line, col; get_line_col(&line, &col);
                preferred_col = col;
                update_scroll(visible_rows);
            } 
            
            else if (c >= 32 && c <= 126) {
                insert_char((char)c);
                int line, col; get_line_col(&line, &col);
                preferred_col = col;
                update_scroll(visible_rows);
            }
        }
    }

    editor_draw(wid);
}

void editor_open(const char *filename) {
    kstrncpy(current_filename, filename, 128);
    wm_create_app(EDITOR_NAME, (rec){40, 40, 600, 400}, C_BG, editor_init, editor_update, editor_draw);
}