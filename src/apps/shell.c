#include "apps/shell.h"
#include "gfx/wm.h"
#include "drivers/keyboard.h"
#include "kernel/io.h"
#include "kernel/sysinfo.h"
#include "fs.h"
#include "lib/kstring.h"

#include "apps/game/game.h"
#include "apps/editor.h"
#include "apps/debug.h"
#include "apps/q3d.h"
#include "apps/explorer/explorer.h"

#define PADDING_X  6
#define PADDING_Y  6
#define LINE_H     (FB_CHAR_H + 2)
#define INPUT_MAX  256
#define LINES_MAX  200
#define LINE_BUF_W 128

#define C_BG     (gfx_color_t){20,  20,  20,  255}
#define C_TEXT   (gfx_color_t){200, 200, 200, 255}
#define C_PROMPT (gfx_color_t){100, 220, 100, 255}
#define C_ERROR  (gfx_color_t){220, 80,  80,  255}
#define C_INPUT  GFX_WHITE
#define C_CURSOR GFX_WHITE
#define C_PATH   (gfx_color_t){100, 180, 255, 255}
#define C_SCROLLBAR_BG        (gfx_color_t){30, 30, 30, 255}
#define C_SCROLLBAR_FG        (gfx_color_t){90, 90, 90, 255}
#define C_SCROLLBAR_FG_ACTIVE (gfx_color_t){130, 130, 130, 255}

#define PATH_MAX 256

extern uint32_t cwd_inode;
extern char cwd_path[PATH_MAX];

#define CMD_IS(s) (cmd_len == (int)(sizeof(s)-1) && kstrncmp(input, s, cmd_len) == 0)

typedef struct {
    char text[LINE_BUF_W];
    gfx_color_t color;
} shell_line_t;

static shell_line_t lines[LINES_MAX];
static int line_count = 0;
static int scroll_top = 0;
static char input_buf[INPUT_MAX];
static int input_len = 0;

static int mouse_left_prev = 0;
static int scrollbar_dragging = 0;

extern mouse_state_t mouse;

static int shell_mouse_down(void) {
    return mouse.left != 0;
}

static int shell_mouse_local_x(int wid) {
    wm_canvas_t canvas = wm_get_canvas(wid);
    return mouse.pos.x - canvas.x;
}

static int shell_mouse_local_y(int wid) {
    wm_canvas_t canvas = wm_get_canvas(wid);
    return mouse.pos.y - canvas.y;
}

typedef struct {
    int cw, ch;
    int visible;
    int total_lines;
    int input_area_h;
    int sb_x, sb_y0, sb_h;
} shell_layout_t;

static shell_layout_t compute_layout(int wid) {
    wm_canvas_t cv = wm_get_canvas(wid);
    shell_layout_t L;

    L.cw = cv.w;
    L.ch = cv.h;
    L.input_area_h = LINE_H + PADDING_Y;
    L.visible = (L.ch - L.input_area_h) / LINE_H;
    if (L.visible < 1) L.visible = 1;
    L.total_lines = line_count;

    L.sb_x = L.cw - 8;
    L.sb_y0 = 0;
    L.sb_h = L.ch - L.input_area_h;
    if (L.sb_h < 1) L.sb_h = 1;

    return L;
}

static void clamp_scroll(shell_layout_t* L) {
    int max_scroll = L->total_lines - L->visible;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_top > max_scroll) scroll_top = max_scroll;
    if (scroll_top < 0) scroll_top = 0;
}

static void scroll_to_bottom(shell_layout_t* L) {
    int max_scroll = L->total_lines - L->visible;
    scroll_top = max_scroll > 0 ? max_scroll : 0;
}

static void shell_handle_mouse(int wid, shell_layout_t* L) {
    int down = shell_mouse_down();
    int just_pressed = down && !mouse_left_prev;
    mouse_left_prev = down;

    if (!down) {
        scrollbar_dragging = 0;
        return;
    }

    int mx = shell_mouse_local_x(wid);
    int my = shell_mouse_local_y(wid);

    int over_scrollbar = mx >= L->sb_x && mx < L->sb_x + 8 &&
                          my >= L->sb_y0 && my < L->sb_y0 + L->sb_h;

    if (scrollbar_dragging || (just_pressed && over_scrollbar)) {
        scrollbar_dragging = 1;

        if (L->total_lines > L->visible) {
            int thumb_h = L->sb_h * L->visible / L->total_lines;
            if (thumb_h < 10) thumb_h = 10;

            int max_track = L->sb_h - thumb_h;
            if (max_track < 1) max_track = 1;

            int max_scroll = L->total_lines - L->visible;

            int rel_y = my - L->sb_y0 - thumb_h / 2;
            int new_scroll = rel_y * max_scroll / max_track;

            if (new_scroll < 0) new_scroll = 0;
            if (new_scroll > max_scroll) new_scroll = max_scroll;
            scroll_top = new_scroll;
        }
    }
}

static void push_line(const char* text, gfx_color_t color) {
    if (line_count < LINES_MAX) {
        kstrncpy(lines[line_count].text, text, LINE_BUF_W);
        lines[line_count].color = color;
        line_count++;
        return;
    }

    for (int i = 0; i < LINES_MAX - 1; i++)
        lines[i] = lines[i + 1];

    kstrncpy(lines[LINES_MAX - 1].text, text, LINE_BUF_W);
    lines[LINES_MAX - 1].color = color;
}

static void shell_print(const char* s, gfx_color_t color) {
    char tmp[LINE_BUF_W];
    int ti = 0;

    while (*s) {
        if (*s == '\n' || ti >= LINE_BUF_W - 1) {
            tmp[ti] = '\0';
            push_line(tmp, color);
            ti = 0;
            if (*s == '\n') { s++; continue; }
        }
        tmp[ti++] = *s++;
    }

    if (ti > 0) {
        tmp[ti] = '\0';
        push_line(tmp, color);
    }
}

void sp(const char* s)  { shell_print(s, C_TEXT);  }
static void spe(const char* s) { shell_print(s, C_ERROR); }

static void read_command(const char* input);

static void cmd_help(const char* args) {
    if (args && kstrcmp(args, "2") == 0) {
        sp("Commands (page 2):");
        sp(" - cat   [file_name]   - output file content");
        sp(" - wr    [file_name]   - write line into file");
        sp(" - run   [file_name]   - run shell file");
        sp(" - mk    [file_name]   - create a file");
        sp(" - mkdir [folder_path] - create a folder");
        sp(" - rm    [file_name]   - remove the file");
        sp(" - rmdir [folder_path] - remove the folder");
        sp(" - lito  [file_name]   - open text editor");
        sp(" - game                - open the example game");
    }

    else {
        sp("Commands:");
        sp(" - help        - commands list");
        sp(" - clear       - clear console");
        sp(" - echo [text] - print text in console");
        sp(" - info        - info about PC");
        sp(" - pwd         - outputs the current path");
        sp(" - ls          - list of files in directory");
        sp(" - cd          - change directory");
        sp(" - reboot      - reboot the PC");
        sp(" - halt        - stop all CPU functions");

        sp("");
        sp("Type 'help 2' to see second page of commands.");
    }
}

static void cmd_cd(const char* args) {
    if (!args || !args[0] || (args[0] == '~' && !args[1])) {
        cwd_inode = 0;
        kstrncpy(cwd_path, "/", PATH_MAX);
        return;
    }

    char resolved[PATH_MAX];
    if (args[0] == '~' && args[1] == '/') {
        resolved[0] = '/';
        kstrncpy(resolved + 1, args + 2, PATH_MAX - 1);
        args = resolved;
    }

    int target = resolve_path(args);
    if (target < 0)         { spe("cd: not found");       return; }
    if (!fs_is_dir(target)) { spe("cd: not a directory"); return; }

    cwd_inode = (uint32_t)target;
    fs_get_path(target, cwd_path, PATH_MAX);
}

static void cmd_ls(const char* args) {
    int target = (args && args[0]) ? resolve_path(args) : (int)cwd_inode;
    if (target < 0) { spe("ls: not found"); return; }

    char names[32][FS_MAX_NAME];
    int count = 0;
    fs_list_names((uint32_t)target, names, &count);

    for (int i = 0; i < count; i++)
        push_line(names[i], C_TEXT);
}

static void cmd_cat(const char* args) {
    if (!args || !args[0]) { spe("cat: missing path"); return; }

    int id = resolve_path(args);
    if (id < 0) { spe("cat: not found"); return; }

    static uint8_t fbuf[512];
    kmemset(fbuf, 0, sizeof(fbuf));
    fs_read_by_id(id, fbuf);
    shell_print((char*)fbuf, C_TEXT);
}

static void cmd_mk(const char* args) {
    if (!args || !args[0]) { spe("mk: missing name"); return; }

    char dir[PATH_MAX], name[FS_MAX_NAME];
    fs_split_path(args, dir, name);

    int parent = dir[0] ? resolve_path(dir) : (int)cwd_inode;
    if (parent < 0) { spe("mk: parent not found"); return; }
    if (fs_mk(name, (uint32_t)parent) < 0) spe("mk: failed");
}

static void cmd_mkdir(const char* args) {
    if (!args || !args[0]) { spe("mkdir: missing name"); return; }

    char dir[PATH_MAX], name[FS_MAX_NAME];
    fs_split_path(args, dir, name);

    int parent = dir[0] ? resolve_path(dir) : (int)cwd_inode;
    if (parent < 0) { spe("mkdir: parent not found"); return; }
    if (fs_mkdir(name, (uint32_t)parent) < 0) spe("mkdir: failed");
}

static void cmd_rm(const char* args) {
    if (!args || !args[0]) { spe("rm: missing path"); return; }

    int id = resolve_path(args);
    if (id < 0)        { spe("rm: not found");  return; }
    if (fs_is_dir(id)) { spe("rm: use rmdir");  return; }
    if (!fs_rm_by_id(id)) spe("rm: failed");
}

static void cmd_rmdir(const char* args) {
    if (!args || !args[0]) { spe("rmdir: missing path"); return; }

    int id = resolve_path(args);
    if (id < 0)           { spe("rmdir: not found");        return; }
    if (!fs_is_dir(id))   { spe("rmdir: not a directory");  return; }
    if (!fs_rmdir_by_id(id)) spe("rmdir: failed");
}


static void cmd_info(void) {
    char buf[128];
    char num[16];

    sp("=== GimmelOS SYSTEM INFO ===");
    sp("");

    kstrcpy(buf, "OS: ", sizeof(buf));
    kstrcat(buf, sys_get_os());
    sp(buf);

    kstrcpy(buf, "Architecture: ", sizeof(buf));
    kstrcat(buf, sys_get_arch());
    sp(buf);

    kstrcpy(buf, "Bootloader: ", sizeof(buf));
    kstrcat(buf, sys_get_bootloader());
    sp(buf);

    sp("");

    kstrcpy(buf, "CPU: ", sizeof(buf));
    kstrcat(buf, sys_get_cpu_name());
    sp(buf);

    kstrcpy(buf, "Vendor: ", sizeof(buf));
    kstrcat(buf, sys_get_cpu_vendor());
    sp(buf);

    if (sys_get_cpu_freq()) {
        kstrcpy(buf, "Frequency: ", sizeof(buf));
        u32toa(sys_get_cpu_freq(), num);
        kstrcat(buf, num);
        kstrcat(buf, " MHz");
        sp(buf);
    }

    kstrcpy(buf, "RAM: ", sizeof(buf));
    u32toa(sys_get_ram_mb(), num);
    kstrcat(buf, num);
    kstrcat(buf, " MB");
    sp(buf);

    kstrcpy(buf, "Screen: ", sizeof(buf));

    u32toa(sys_get_screen_width(), num);
    kstrcat(buf, num);
    kstrcat(buf, "x");

    u32toa(sys_get_screen_height(), num);
    kstrcat(buf, num);
    kstrcat(buf, " ");

    sp(buf);

    sp("");
    sp("============================");
}

void cmd_lito(const char* args) {
    if (!args || !args[0]) { spe("lito: missing path"); return; }

    int id = resolve_path(args);
    if (id < 0) { spe("lito: not found"); return; }
    if (fs_is_dir(id)) { spe("lito: is a directory"); return; }

    editor_open(id);
}

static void cmd_wolfenstein(void) { wm_create_app("minekampf", (rec){100, 100, 200, 200}, GFX_SKY_BLUE, game_init, game_update, game_draw); }
static void cmd_explorer(void) { wm_create_app("Explorer", (rec){100, 100, 500, 350}, GFX_BLACK, explorer_init, explorer_update, explorer_draw); }
static void cmd_debug(void) { wm_create_app("Debug", (rec){100, 100, 200, 200}, GFX_BLACK, debug_init, debug_update, debug_draw); }
static void cmd_q3d(void) { wm_create_app("rotating cube", (rec){200, 200, 200, 200}, GFX_BLACK, q3d_init, q3d_update, q3d_draw); }

static void run_gim(const char* args) {
    if (!args || !args[0]) { spe("run: missing path"); return; }

    int id = resolve_path(args);
    if (id < 0) { spe("run: not found"); return; }

    static uint8_t rbuf[512];
    kmemset(rbuf, 0, sizeof(rbuf));
    if (!fs_read_by_id(id, rbuf)) { spe("run: read failed"); return; }

    char* line = (char*)rbuf;
    char* start = line;

    for (int i = 0; i < 512 && line[i]; i++) {
        if (line[i] == '\r') line[i] = '\0';
        if (line[i] == '\n' || line[i] == '\0') {
            line[i] = '\0';
            while (*start == ' ' || *start == '\r') start++;
            if (*start) read_command(start);
            start = &line[i + 1];
        }
    }

    while (*start == ' ') start++;
    if (*start) read_command(start);
}

static void reboot(void) {
    uint8_t good = 0x02;
    while (good & 0x02) __asm__ volatile("inb $0x64,%0" : "=a"(good));
    __asm__ volatile("outb %0,$0x64" :: "a"((uint8_t)0xFE));
    __asm__ volatile("hlt");
}

static void read_command(const char* input) {
    if (!input || !input[0]) return;

    int cmd_len = 0;
    while (input[cmd_len] && input[cmd_len] != ' ') cmd_len++;
    const char* args = (input[cmd_len] == ' ') ? &input[cmd_len + 1] : "";

    char echo[LINE_BUF_W];
    int ei = kstrlen(cwd_path);
    kstrncpy(echo, cwd_path, LINE_BUF_W);
    echo[ei++] = ' '; echo[ei++] = '>'; echo[ei++] = ' ';
    kstrncpy(echo + ei, input, LINE_BUF_W - ei);
    push_line(echo, C_PROMPT);

    if      (CMD_IS("help"))        cmd_help(args);
    else if (CMD_IS("clear"))       { line_count = 0; scroll_top = 0; }
    else if (CMD_IS("echo"))        sp(args);
    else if (CMD_IS("info"))        cmd_info();
    else if (CMD_IS("pwd"))         sp(cwd_path);
    else if (CMD_IS("reboot"))      reboot();
    else if (CMD_IS("halt"))        { sp("Halting."); __asm__ volatile("cli;hlt"); }
    else if (CMD_IS("ls"))          cmd_ls(args);
    else if (CMD_IS("cd"))          cmd_cd(args);
    else if (CMD_IS("lito"))        cmd_lito(args);
    else if (CMD_IS("mkdir"))       cmd_mkdir(args);
    else if (CMD_IS("mk"))          cmd_mk(args);
    else if (CMD_IS("rm"))          cmd_rm(args);
    else if (CMD_IS("rmdir"))       cmd_rmdir(args);
    else if (CMD_IS("cat"))         cmd_cat(args);
    else if (CMD_IS("run"))         run_gim(args);
    else if (CMD_IS("explorer"))    cmd_explorer();
    else if (CMD_IS("debug"))       cmd_debug();
    else if (CMD_IS("q3d"))         cmd_q3d();
    else if (CMD_IS("game"))        cmd_wolfenstein();
    else {
        char tmp[LINE_BUF_W];
        kstrncpy(tmp, "Unknown: ", LINE_BUF_W);
        kstrncpy(tmp + 9, input, LINE_BUF_W - 9);
        spe(tmp);
    }
}

void shell_init(int wid) {
    (void)wid;
    line_count   = 0;
    scroll_top   = 0;
    input_len    = 0;
    input_buf[0] = '\0';
    mouse_left_prev = 0;
    scrollbar_dragging = 0;

    push_line("+---------------------------------+", C_PROMPT);
    push_line("|         GimmelOS  v0.1          |", C_TEXT);
    push_line("+---------------------------------+", C_PROMPT);
    push_line("Type 'help' for commands.", C_TEXT);
    push_line("", C_TEXT);
}

void shell_update(int wid) {
    extern wm_t wm;
    if (wm.focused != wid) return;

    shell_layout_t layout = compute_layout(wid);
    shell_handle_mouse(wid, &layout);

    int c;
    while ((c = keyboard_getchar_nonblocking())) {
        if (c == '\n' || c == '\r') {
            input_buf[input_len] = '\0';
            read_command(input_buf);
            input_len    = 0;
            input_buf[0] = '\0';

            layout = compute_layout(wid);
            scroll_to_bottom(&layout);
        }

        else if (c == '\b') {
            if (input_len > 0) input_buf[--input_len] = '\0';
        }

        else if (c >= 32 && c <= 126 && input_len < INPUT_MAX - 1) {
            input_buf[input_len++] = (char)c;
            input_buf[input_len]   = '\0';
        }
    }

    clamp_scroll(&layout);
}

void shell_draw(int wid) {
    shell_layout_t L = compute_layout(wid);
    int cw = L.cw, ch = L.ch;

    wm_begin_draw(wid);
    wm_draw_fill_rec((rec){0, 0, cw, ch}, C_BG);

    for (int i = 0; i < L.visible; i++) {
        int idx = scroll_top + i;
        if (idx >= line_count) break;
        wm_draw_text(lines[idx].text, (vec2){PADDING_X, PADDING_Y + i * LINE_H}, lines[idx].color);
    }

    wm_draw_fill_rec((rec){L.sb_x, L.sb_y0, 8, L.sb_h}, C_SCROLLBAR_BG);

    if (L.total_lines > L.visible) {
        int thumb_h = L.sb_h * L.visible / L.total_lines;
        if (thumb_h < 10) thumb_h = 10;

        int max_scroll = L.total_lines - L.visible;
        int max_track = L.sb_h - thumb_h;
        if (max_track < 1) max_track = 1;

        int thumb_y = L.sb_y0 + max_track * scroll_top / (max_scroll > 0 ? max_scroll : 1);

        wm_draw_fill_rec((rec){L.sb_x + 1, thumb_y, 6, thumb_h},
            scrollbar_dragging ? C_SCROLLBAR_FG_ACTIVE : C_SCROLLBAR_FG);
    }

    int sep_y = ch - L.input_area_h;
    wm_draw_line((vec2){0, sep_y}, (vec2){cw, sep_y}, (gfx_color_t){50, 50, 50, 255});

    int py = sep_y + PADDING_Y / 2;
    int px = PADDING_X;

    wm_draw_text(cwd_path, (vec2){px, py}, C_PATH);
    px += kstrlen(cwd_path) * FB_CHAR_W;

    wm_draw_text(" > ", (vec2){px, py}, C_PROMPT);
    px += 3 * FB_CHAR_W;

    wm_draw_text(input_buf, (vec2){px, py}, C_INPUT);
    px += input_len * FB_CHAR_W;

    wm_draw_fill_rec((rec){px, py, 2, FB_CHAR_H}, C_CURSOR);
    wm_end_draw();
}