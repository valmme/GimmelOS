#include "apps/explorer.h"
#include "fs.h"
#include "gfx/wm.h"
#include "gfx/icons.h"
#include "drivers/keyboard.h"
#include "lib/kstring.h"

#define COLOR_BG          ((gfx_color_t){30, 30, 30, 255})
#define COLOR_TEXT_FILE   ((gfx_color_t){230, 230, 230, 255})
#define COLOR_TEXT_DIR    ((gfx_color_t){240, 200, 80, 255})
#define COLOR_SELECTED_BG ((gfx_color_t){50, 80, 140, 255})
#define COLOR_PATH_BG     ((gfx_color_t){20, 20, 20, 255})

#define ITEM_Y0 20
#define ITEM_H  34

#define ICON_PAD_X      4
#define ICON_PAD_Y      1
#define ICON_TEXT_GAP   4
#define TEXT_X_OFFSET   (ICON_PAD_X + FOLDER_W + ICON_TEXT_GAP)

extern mouse_state_t mouse;
extern wm_t wm;

uint32_t current_path;

static char file_names[FS_MAX_INODES][FS_MAX_NAME];
static int file_count = 0;
static int selected_idx = 0;
static int prev_left = 0;

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
    if (id >= 0 && fs_is_dir(id)) {
        current_path = id;
        selected_idx = 0;
        refresh_file_list();
    }
}

void explorer_init(int wid) {
    current_path = 0;
    selected_idx = 0;
    prev_left = 0;
    refresh_file_list();
}

void explorer_update(int wid) {
    int key = keyboard_getchar_nonblocking();

    if (key) {
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

    if (wm.focused == wid) {
        wm_canvas_t canvas = wm_get_canvas(wid);
        int rel_x = mouse.pos.x - canvas.x;
        int rel_y = mouse.pos.y - canvas.y;

        int hit_idx = -1;
        int item_y = ITEM_Y0;

        for (int i = 0; i < file_count; i++) {
            if (item_y + ITEM_H > (int32_t)canvas.h) break;

            if (rel_x >= 2 && rel_x < (int32_t)canvas.w - 2 &&
                rel_y >= item_y && rel_y < item_y + ITEM_H) {
                hit_idx = i;
                break;
            }

            item_y += ITEM_H;
        }

        if (hit_idx >= 0 && mouse.left && !prev_left) {
            if (hit_idx == selected_idx) activate_selected();
            else selected_idx = hit_idx;
        }
    }

    prev_left = mouse.left;
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
    wm_draw_text(path_str, (vec2){4, 4}, GFX_WHITE, COLOR_PATH_BG);

    int item_y = ITEM_Y0;

    if (file_count == 0) {
        wm_draw_text("Empty Directory", (vec2){10, item_y}, GFX_GRAY, COLOR_BG);
    }

    else {
        for (int i = 0; i < file_count; i++) {
            if (item_y + ITEM_H > (int32_t)canvas.h) break;

            rec item_rec = {2, item_y, canvas.w - 4, (uint32_t)ITEM_H};
            int is_selected = (i == selected_idx);

            gfx_color_t item_bg = is_selected ? COLOR_SELECTED_BG : COLOR_BG;
            if (is_selected) {
                wm_draw_fill_rec(item_rec, item_bg);
            }

            gfx_color_t text_color = COLOR_TEXT_FILE;
            char display_name[FS_MAX_NAME + 4];
            int draw_folder_icon = 0;

            if (kstrcmp(file_names[i], "..") == 0) {
                text_color = COLOR_TEXT_DIR;
                kstrncpy(display_name, "..", sizeof(display_name));
                draw_folder_icon = 1;
            }

            else if (name_has_dir_slash(file_names[i])) {
                text_color = COLOR_TEXT_DIR;
                kstrncpy(display_name, file_names[i], sizeof(display_name));
                strip_dir_slash(display_name);
                draw_folder_icon = 1;
            }

            else {
                kstrncpy(display_name, file_names[i], sizeof(display_name));
                wm_draw_texture(txt_file_icon, (vec2){ICON_PAD_X, item_y + ICON_PAD_Y}, (vec2){FILE_W, FILE_H});
            }

            if (draw_folder_icon) {
                wm_draw_texture(folder_icon, (vec2){ICON_PAD_X, item_y + ICON_PAD_Y}, (vec2){FOLDER_W, FOLDER_H});
            }

            int text_y = item_y + (ITEM_H - FB_CHAR_H) / 2;
            wm_draw_text(display_name, (vec2){TEXT_X_OFFSET, text_y}, text_color, item_bg);
            item_y += ITEM_H;
        }
    }

    wm_end_draw();
}