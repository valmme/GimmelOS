#include "apps/explorer/explorer.h"
#include "apps/shell.h"
#include "fs.h"
#include "gfx/wm.h"
#include "apps/explorer/icons.h"
#include "drivers/keyboard.h"
#include "lib/kstring.h"
#include "lib/math.h"

#define COLOR_BG            ((gfx_color_t){40, 40, 40, 255})
#define COLOR_TEXT          ((gfx_color_t){220, 220, 220, 255})
#define COLOR_TEXT_SELECTED ((gfx_color_t){255, 255, 255, 255})
#define COLOR_FACE          ((gfx_color_t){60, 60, 60, 255})
#define COLOR_HILIGHT       ((gfx_color_t){255, 255, 255, 255})
#define COLOR_SHADOW        ((gfx_color_t){15, 15, 15, 255})
#define COLOR_PATH_BG       COLOR_FACE
#define COLOR_TOOLBAR_BG    COLOR_FACE
#define COLOR_SEL_NEAR      ((gfx_color_t){50, 100, 210, 255})
#define COLOR_SEL_FAR       ((gfx_color_t){10, 30, 90, 255})

#define TOOLBAR_MARGIN 8
#define BTN_GAP        6
#define TOOLBAR_Y      20
#define TOOLBAR_H      30
#define TOOLBAR_PANEL_H (TOOLBAR_H + 8)

#define LIST_TOP    (16 + TOOLBAR_PANEL_H)
#define ITEM_Y0     (LIST_TOP + 5)
#define ITEM_H      34

#define ICON_PAD_X      4
#define ICON_PAD_Y      1
#define ICON_TEXT_GAP   4
#define TEXT_X_OFFSET   (ICON_PAD_X + ICON_W + ICON_TEXT_GAP)

#define MENU_W      110
#define MENU_ROW_H  30
#define MENU_ROWS   3

enum mode_e {
    MODE_NORMAL,
    MODE_CREATE_FILE,
    MODE_CREATE_FOLDER,
    MODE_RENAME
};

extern mouse_state_t mouse;
extern wm_t wm;

uint32_t current_path;

static char file_names[FS_MAX_INODES][FS_MAX_NAME];
static int file_count = 0;
static int selected_idx = 0;

static int prev_right = 0;
static int prev_left = 0;

static int menu_open = 0;
static int menu_x, menu_y;

static int mode = MODE_NORMAL;
static char input_buffer[FS_MAX_NAME];
static int input_len = 0;

static int clipboard_valid = 0;
static int clipboard_id = -1;
static char clipboard_name[FS_MAX_NAME];

static int ends_with(const char *str, const char *ext) {
    int slen = kstrlen(str);
    int elen = kstrlen(ext);
    if (slen < elen) return 0;

    return kstrcmp(str + slen - elen, ext) == 0;
}

static int name_has_dir_slash(const char* name) {
    int len = kstrlen(name);
    return len > 0 && name[len - 1] == '/';
}

static void strip_dir_slash(char* name) {
    int len = kstrlen(name);
    if (len > 0 && name[len - 1] == '/') name[len - 1] = '\0';
}

static void refresh_file_list() {
    file_count = 0;

    if (current_path != 0) {
        kstrncpy(file_names[0], "..", FS_MAX_NAME);
        file_count = 1;
    }

    int fs_count = 0;
    char temp_names[FS_MAX_INODES][FS_MAX_NAME];
    fs_list_names(current_path, temp_names, &fs_count);

    for (int i = 0; i < fs_count; i++) {
        if (file_count < FS_MAX_INODES) {
            kstrncpy(file_names[file_count], temp_names[i], FS_MAX_NAME);
            file_count++;
        }
    }

    if (selected_idx >= file_count) {
        selected_idx = 0;
    }
}

static void append_int(char* buf, int n) {
    char tmp[12];
    int i = 0;

    if (n == 0) {
        tmp[i++] = '0';
    }

    while (n > 0) {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    }

    int len = kstrlen(buf);
    for (int j = i - 1; j >= 0; j--) buf[len++] = tmp[j];
    buf[len] = '\0';
}

static void make_unique_name(char* out, const char* base, uint32_t parent) {
    kstrncpy(out, base, FS_MAX_NAME);
    int n = 2;

    while (fs_find_in(out, parent) >= 0) {
        kstrncpy(out, base, FS_MAX_NAME);
        kstrncat(out, "_copy", FS_MAX_NAME);

        if (n > 2) append_int(out, n);

        n++;
    }
}

static void activate_selected(void) {
    if (file_count == 0) return;

    if (kstrcmp(file_names[selected_idx], "..") == 0) {
        current_path = fs_get_parent(current_path);
        selected_idx = 0;
        refresh_file_list();
        return;
    }

    char lookup_name[FS_MAX_NAME];
    kstrncpy(lookup_name, file_names[selected_idx], FS_MAX_NAME);
    strip_dir_slash(lookup_name);

    int id = fs_find_in(lookup_name, current_path);
    if (id >= 0) {
        if (fs_is_dir(id)) {
            current_path = id;
            selected_idx = 0;
            refresh_file_list();
        }

        else {
            editor_open(id);
        }
    }
}

static void on_btn_create_file(void) {
    mode = MODE_CREATE_FILE;
    input_len = 0;
    input_buffer[0] = '\0';
    menu_open = 0;
}

static void on_btn_create_folder(void) {
    mode = MODE_CREATE_FOLDER;
    input_len = 0;
    input_buffer[0] = '\0';
    menu_open = 0;
}

static void on_btn_rename(void) {
    if (file_count == 0) return;
    if (kstrcmp(file_names[selected_idx], "..") == 0) return;

    kstrncpy(input_buffer, file_names[selected_idx], FS_MAX_NAME);
    strip_dir_slash(input_buffer);
    input_len = kstrlen(input_buffer);
    mode = MODE_RENAME;
    menu_open = 0;
}

static void on_btn_copy(void) {
    if (file_count == 0) return;
    if (kstrcmp(file_names[selected_idx], "..") == 0) return;

    char name[FS_MAX_NAME];
    kstrncpy(name, file_names[selected_idx], FS_MAX_NAME);
    strip_dir_slash(name);

    int id = fs_find_in(name, current_path);
    if (id < 0) return;

    clipboard_id = id;
    kstrncpy(clipboard_name, name, FS_MAX_NAME);
    clipboard_valid = 1;
    menu_open = 0;
}

static void on_btn_paste(void) {
    if (!clipboard_valid) return;

    char target_name[FS_MAX_NAME];
    make_unique_name(target_name, clipboard_name, current_path);

    fs_copy_by_id(clipboard_id, current_path, target_name);
    refresh_file_list();
    menu_open = 0;
}

static void delete_selected(void) {
    if (kstrcmp(file_names[selected_idx], "..") == 0) return;

    char name[FS_MAX_NAME];
    kstrncpy(name, file_names[selected_idx], FS_MAX_NAME);
    strip_dir_slash(name);

    int id = fs_find_in(name, current_path);
    if (id < 0) return;

    if (fs_is_dir(id)) fs_rmdir(name, current_path);
    else fs_rm(name, current_path);

    refresh_file_list();
}

void explorer_init(int wid) {
    current_path = 0;
    selected_idx = 0;
    prev_left = 0;
    prev_right = 0;
    menu_open = 0;
    mode = MODE_NORMAL;
    clipboard_valid = 0;

    int bx = TOOLBAR_MARGIN;

    int w1 = 75; wm_add_button(wid, "New File",   (rec){bx, TOOLBAR_Y, w1, TOOLBAR_H - 5}, on_btn_create_file);   bx += w1 + BTN_GAP;
    int w2 = 90; wm_add_button(wid, "New Folder", (rec){bx, TOOLBAR_Y, w2, TOOLBAR_H - 5}, on_btn_create_folder); bx += w2 + BTN_GAP;
    int w3 = 65; wm_add_button(wid, "Rename",     (rec){bx, TOOLBAR_Y, w3, TOOLBAR_H - 5}, on_btn_rename);        bx += w3 + BTN_GAP;
    int w4 = 55; wm_add_button(wid, "Copy",       (rec){bx, TOOLBAR_Y, w4, TOOLBAR_H - 5}, on_btn_copy);          bx += w4 + BTN_GAP;
    int w5 = 55; wm_add_button(wid, "Paste",      (rec){bx, TOOLBAR_Y, w5, TOOLBAR_H - 5}, on_btn_paste);

    refresh_file_list();
}

static void handle_input_key(int key) {
    if (key == '\n' || key == '\r') {
        if (input_len > 0) {
            if (mode == MODE_CREATE_FILE) {
                if (fs_find_in(input_buffer, current_path) < 0)
                    fs_mk(input_buffer, current_path);
            }

            else if (mode == MODE_CREATE_FOLDER) {
                if (fs_find_in(input_buffer, current_path) < 0)
                    fs_mkdir(input_buffer, current_path);
            }

            else if (mode == MODE_RENAME) {
                char old_name[FS_MAX_NAME];
                kstrncpy(old_name, file_names[selected_idx], FS_MAX_NAME);
                strip_dir_slash(old_name);

                int id = fs_find_in(old_name, current_path);
                if (id >= 0 && fs_find_in(input_buffer, current_path) < 0) {
                    fs_rename_by_id(id, input_buffer);
                }
            }

            refresh_file_list();
        }

        mode = MODE_NORMAL;
    }

    else if (key == '\b') {
        if (input_len > 0) {
            input_len--;
            input_buffer[input_len] = '\0';
        }

        else {
            mode = MODE_NORMAL;
        }
    }

    else if (key >= 32 && key < 127 && input_len < FS_MAX_NAME - 1) {
        input_buffer[input_len++] = (char)key;
        input_buffer[input_len] = '\0';
    }
}

void explorer_update(int wid) {
    int key = keyboard_getchar_nonblocking();

    if (key) {
        if (mode != MODE_NORMAL) {
            handle_input_key(key);
        }

        else {
            switch (key) {
                case KEY_DOWN:
                    if (file_count > 0) selected_idx = (selected_idx + 1) % file_count;
                    break;

                case KEY_UP:
                    if (file_count > 0) selected_idx = (selected_idx - 1 + file_count) % file_count;
                    break;

                case '\n':
                case '\r':
                    activate_selected();
                    break;

                case '\b':
                    if (current_path != 0) {
                        current_path = fs_get_parent(current_path);
                        selected_idx = 0;
                        refresh_file_list();
                    }
                    break;
            }
        }
    }

    if (wm.focused == wid && mode == MODE_NORMAL) {
        wm_canvas_t canvas = wm_get_canvas(wid);
        int rel_x = mouse.pos.x - canvas.x;
        int rel_y = mouse.pos.y - canvas.y;

        if (!menu_open) {
            int hit_idx = -1;
            int item_y = ITEM_Y0;

            for (int i = 0; i < file_count; i++) {
                if (item_y + ITEM_H > (int32_t)canvas.h) break;

                if (rel_x >= 4 && rel_x < (int32_t)canvas.w - 4 && rel_y >= item_y && rel_y < item_y + ITEM_H) {
                    hit_idx = i;
                    break;
                }

                item_y += ITEM_H;
            }

            if (hit_idx >= 0 && mouse.left && !prev_left) {
                if (hit_idx == selected_idx) activate_selected();
                else selected_idx = hit_idx;
            }

            if (hit_idx >= 0 && mouse.right && !prev_right) {
                selected_idx = hit_idx;
                menu_open = 1;
                menu_x = rel_x;
                menu_y = rel_y;
            }
        }

        else {
            if (mouse.left && !prev_left) {
                if (rel_x > menu_x && rel_x < menu_x + MENU_W &&
                    rel_y > menu_y && rel_y < menu_y + MENU_ROW_H * MENU_ROWS &&
                    kstrcmp(file_names[selected_idx], "..") != 0) {

                    int row = (rel_y - menu_y) / MENU_ROW_H;

                    if (row == 0) delete_selected();
                    else if (row == 1) on_btn_rename();
                    else if (row == 2) on_btn_copy();
                }

                menu_open = 0;
            }
        }
    }

    prev_right = mouse.right;
    prev_left = mouse.left;
}

static void draw_bevel_rec(rec r, int raised) {
    gfx_color_t hi = raised ? COLOR_HILIGHT : COLOR_SHADOW;
    gfx_color_t sh = raised ? COLOR_SHADOW : COLOR_HILIGHT;

    wm_draw_line((vec2){r.x, r.y}, (vec2){r.x + (int32_t)r.w - 1, r.y}, hi);
    wm_draw_line((vec2){r.x, r.y}, (vec2){r.x, r.y + (int32_t)r.h - 1}, hi);
    wm_draw_line((vec2){r.x + (int32_t)r.w - 1, r.y}, (vec2){r.x + (int32_t)r.w - 1, r.y + (int32_t)r.h - 1}, sh);
    wm_draw_line((vec2){r.x, r.y + (int32_t)r.h - 1}, (vec2){r.x + (int32_t)r.w - 1, r.y + (int32_t)r.h - 1}, sh);
}

void explorer_draw(int wid) {
    wm_begin_draw(wid);

    wm_canvas_t canvas = wm_get_canvas(wid);
    wm_draw_fill_rec((rec){0, 0, canvas.w, canvas.h}, COLOR_BG);

    char path_str[128];
    kstrncpy(path_str, " Path: /", 128);

    char fs_path[96];
    fs_get_path(current_path, fs_path, 96);
    kstrncat(path_str, fs_path, 128);

    wm_draw_fill_rec((rec){0, 0, canvas.w, 16}, COLOR_PATH_BG);
    wm_draw_text(path_str, (vec2){4, 4}, COLOR_TEXT);

    wm_draw_fill_rec((rec){0, 16, canvas.w, TOOLBAR_H + 4}, COLOR_TOOLBAR_BG);
    draw_bevel_rec((rec){0, 16, canvas.w, TOOLBAR_H + 4}, 1);

    rec list_border = {0, LIST_TOP, canvas.w, canvas.h - LIST_TOP};
    draw_bevel_rec(list_border, 0);

    int item_y = ITEM_Y0;

    if (file_count == 0) {
        wm_draw_text("Empty Directory", (vec2){10, item_y}, COLOR_TEXT);
    }

    else {
        for (int i = 0; i < file_count; i++) {
            if (item_y + ITEM_H > (int32_t)canvas.h - 4) break;

            rec item_rec = {4, item_y, canvas.w - 8, (uint32_t)ITEM_H};
            int is_selected = (i == selected_idx);

            gfx_color_t item_bg = is_selected ? COLOR_SEL_NEAR : COLOR_BG;

            if (is_selected) {
                wm_draw_fill_rec(item_rec, COLOR_SEL_NEAR);
            }

            vec2 rel_mouse = { mouse.pos.x - canvas.x, mouse.pos.y - canvas.y };

            if (check_collision_rec(rel_mouse, item_rec) && wm.focused == wid) {
                wm_draw_rec(item_rec, COLOR_HILIGHT);
            }

            gfx_color_t text_color = is_selected ? COLOR_TEXT_SELECTED : COLOR_TEXT;
            char display_name[FS_MAX_NAME + 4];
            int draw_folder_icon = 0;

            if (kstrcmp(file_names[i], "..") == 0) {
                kstrncpy(display_name, "..", sizeof(display_name));
                draw_folder_icon = 1;
            }

            else if (name_has_dir_slash(file_names[i])) {
                kstrncpy(display_name, file_names[i], sizeof(display_name));
                strip_dir_slash(display_name);
                draw_folder_icon = 1;
            }

            else {
                kstrncpy(display_name, file_names[i], sizeof(display_name));
                uint32_t icon[ICON_W * ICON_H];

                if (ends_with(display_name, ".c"))        kmemcpy(icon, c_file_icon, sizeof(icon));
                else if (ends_with(display_name, ".h"))   kmemcpy(icon, h_file_icon, sizeof(icon));
                else if (ends_with(display_name, ".cpp")) kmemcpy(icon, cpp_file_icon, sizeof(icon));
                else if (ends_with(display_name, ".hpp")) kmemcpy(icon, hpp_file_icon, sizeof(icon));
                else kmemcpy(icon, file_icon, sizeof(icon));

                wm_draw_texture(icon, (vec2){item_rec.x + ICON_PAD_X, item_y + ICON_PAD_Y}, (vec2){ICON_W, ICON_H});
            }

            if (draw_folder_icon) {
                wm_draw_texture(folder_icon, (vec2){item_rec.x + ICON_PAD_X, item_y + ICON_PAD_Y}, (vec2){ICON_W, ICON_H});
            }

            int text_y = item_y + (ITEM_H - FB_CHAR_H) / 2;
            wm_draw_text(display_name, (vec2){item_rec.x + TEXT_X_OFFSET, text_y}, text_color);
            item_y += ITEM_H;
        }
    }

    if (menu_open) {
        rec menu_rect = {menu_x, menu_y, MENU_W, MENU_ROW_H * MENU_ROWS};
        wm_draw_fill_rec(menu_rect, COLOR_FACE);
        draw_bevel_rec(menu_rect, 1);

        vec2 rel_mouse = { mouse.pos.x - canvas.x, mouse.pos.y - canvas.y };
        int hover_row = -1;

        if (rel_mouse.x > menu_x + 2 && rel_mouse.x < menu_x + MENU_W - 2 &&
            rel_mouse.y > menu_y + 2 && rel_mouse.y < menu_y + MENU_ROW_H * MENU_ROWS - 2) {
            hover_row = (rel_mouse.y - menu_y) / MENU_ROW_H;
        }

        const char* labels[MENU_ROWS] = {"Delete", "Rename", "Copy"};

        for (int r = 0; r < MENU_ROWS; r++) {
            rec row_rect = {menu_x + 2, menu_y + 2 + r * MENU_ROW_H, MENU_W - 4, MENU_ROW_H - (r == MENU_ROWS - 1 ? 2 : 0)};
            gfx_color_t row_bg = (r == hover_row) ? COLOR_SEL_FAR : COLOR_FACE;
            gfx_color_t row_fg = (r == hover_row) ? COLOR_TEXT_SELECTED : COLOR_TEXT;

            wm_draw_fill_rec(row_rect, row_bg);
            wm_draw_text(labels[r], (vec2){menu_x + 10, menu_y + 2 + r * MENU_ROW_H + 9}, row_fg);

            if (r < MENU_ROWS - 1) {
                wm_draw_line(
                    (vec2){menu_x + 2, menu_y + (r + 1) * MENU_ROW_H},
                    (vec2){menu_x + MENU_W - 3, menu_y + (r + 1) * MENU_ROW_H},
                    COLOR_SHADOW
                );
            }
        }
    }

    if (mode != MODE_NORMAL) {
        int box_w = canvas.w > 220 ? 220 : (int32_t)canvas.w - 20;
        rec box = {40, 60, (uint32_t)box_w, 50};

        wm_draw_fill_rec(box, COLOR_FACE);
        draw_bevel_rec(box, 1);

        const char* label = mode == MODE_CREATE_FILE   ? "New file name:" : mode == MODE_CREATE_FOLDER ? "New folder name:" : "Rename to:";

        wm_draw_text(label, (vec2){box.x + 6, box.y + 8}, COLOR_TEXT);

        rec input_rect = {box.x + 6, box.y + 22, box.w - 12, 18};
        wm_draw_fill_rec(input_rect, COLOR_SHADOW);
        draw_bevel_rec(input_rect, 0);
        wm_draw_text(input_buffer, (vec2){input_rect.x + 2, input_rect.y + 2}, COLOR_TEXT_SELECTED);
    }

    wm_end_draw();
}