#include "apps/shell.h"
#include "gfx/wm.h"
#include "drivers/keyboard.h"
#include "fs.h"
#include "lib/kstring.h"

#include "apps/game/game.h"
#include "apps/editor.h"
#include "apps/explorer/explorer.h"

#define CHAR_W     8
#define CHAR_H     8
#define PADDING_X  6
#define PADDING_Y  6
#define LINE_H     10
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

static void cpuid_call(uint32_t code, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    __asm__ volatile("cpuid" : "=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d) : "a"(code));
}

static void cmd_help(void) {
    sp("Commands:");
    sp(" - help      - commands list");
    sp(" - clear     - clear console");
    sp(" - echo      - print text in console");
    sp(" - info      - info about PC");
    sp(" - pwd       - outputs the current path");
    sp(" - ls        - list of files in directory");
    sp(" - cd        - change directory");
    sp(" - cat       - output file content");
    sp(" - wr        - write line into file");
    sp(" - run       - run shell file");
    sp(" - mk     ");
    sp(" - mkdir");
    sp(" - rm");
    sp(" - rmdir");
    sp(" - lito [path]  - open editor");
    sp(" - wolfenstein (open the game)");
    sp(" - reboot");
    sp(" - halt");
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

    char line[LINE_BUF_W];
    line[0] = '\0';
    int lx = 0;

    for (int i = 0; i < count; i++) {
        int nlen = kstrlen(names[i]);
        if (lx + nlen + 2 >= LINE_BUF_W) {
            push_line(line, C_TEXT);
            line[0] = '\0';
            lx = 0;
        }
        kstrncpy(line + lx, names[i], LINE_BUF_W - lx);
        lx += nlen;
        line[lx++] = '\n';
        line[lx] = '\0';
    }

    if (lx > 0) push_line(line, C_TEXT);
}

static void cmd_cat(const char* args) {
    if (!args || !args[0]) { spe("cat: missing path"); return; }

    int id = resolve_path(args);
    if (id < 0) { spe("cat: not found"); return; }

    static uint8_t fbuf[512];
    kmemset(fbuf, 0, sizeof(fbuf));
    fs_read_by_id(id, fbuf);
    fbuf[511] = '\0';
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
    sp("=== GimmelOS SYSTEM INFO ===");
    sp("OS: GimmelOS v0.1     |  Arch: x86 32-bit");
    sp("Boot: GRUB Multiboot  |  GFX: framebuffer");
    sp("Input: PS/2 polling   |  Mem: flat, no paging");

    uint32_t a, b, c, d;
    cpuid_call(0, &a, &b, &c, &d);

    static const char hex[] = "0123456789ABCDEF";
    char tmp[64];
    kstrncpy(tmp, "CPU eax=0x", 64);
    int ti = kstrlen(tmp);
    for (int sh = 28; sh >= 0; sh -= 4) tmp[ti++] = hex[(a >> sh) & 0xF];
    tmp[ti] = '\0';
    sp(tmp);
    sp("============================");
}

void cmd_lito(const char* args) {
    if (!args || !args[0]) { spe("lito: missing path"); return; }

    char fname[FS_MAX_NAME];
    kstrncpy(fname, args, FS_MAX_NAME);
    editor_open(fname);
}

static void cmd_wolfenstein(void) {
    wm_create_app("Wolfenstein", (rec){100, 100, 200, 200}, GFX_BLACK, game_init, game_update, game_draw);
}

static void cmd_explorer(void) {
    wm_create_app("Explorer", (rec){100, 100, 200, 200}, GFX_BLACK, explorer_init, explorer_update, explorer_draw);
}

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

    if      (CMD_IS("help"))        cmd_help();
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
    else if (CMD_IS("wolfenstein")) cmd_wolfenstein();
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

    push_line("+---------------------------------+", C_PROMPT);
    push_line("|       GimmelOS  v0.1            |", C_TEXT);
    push_line("+---------------------------------+", C_PROMPT);
    push_line("Type 'help' for commands.", C_TEXT);
    push_line("", C_TEXT);
}

void shell_update(int wid) {
    extern wm_t wm;
    if (wm.focused != wid) return;

    int c;
    while ((c = keyboard_getchar_nonblocking())) {
        if (c == '\n' || c == '\r') {
            input_buf[input_len] = '\0';
            read_command(input_buf);
            input_len    = 0;
            input_buf[0] = '\0';

            wm_canvas_t cv = wm_get_canvas(wid);
            int visible = (cv.h - PADDING_Y * 2 - LINE_H) / LINE_H;
            if (visible < 1) visible = 1;
            scroll_top = line_count - visible;
            if (scroll_top < 0) scroll_top = 0;

        } 
        
        else if (c == '\b') {
            if (input_len > 0) input_buf[--input_len] = '\0';

        } 
        
        else if (c >= 32 && c <= 126 && input_len < INPUT_MAX - 1) {
            input_buf[input_len++] = (char)c;
            input_buf[input_len]   = '\0';
        }
    }
}

void shell_draw(int wid) {
    wm_canvas_t cv = wm_get_canvas(wid);
    int cw = cv.w, ch = cv.h;

    wm_begin_draw(wid);
    wm_draw_fill_rec((rec){0, 0, cw, ch}, C_BG);

    int input_area_h = LINE_H + PADDING_Y;
    int visible = (ch - input_area_h) / LINE_H;
    if (visible < 1) visible = 1;

    for (int i = 0; i < visible; i++) {
        int idx = scroll_top + i;
        if (idx >= line_count) break;
        wm_draw_text(lines[idx].text, (vec2){PADDING_X, PADDING_Y + i * LINE_H}, lines[idx].color, C_BG);
    }

    int sep_y = ch - input_area_h;
    wm_draw_line((vec2){0, sep_y}, (vec2){cw, sep_y}, (gfx_color_t){50, 50, 50, 255});

    int py = sep_y + PADDING_Y / 2;
    int px = PADDING_X;

    wm_draw_text(cwd_path, (vec2){px, py}, C_PATH, C_BG);
    px += kstrlen(cwd_path) * CHAR_W;

    wm_draw_text(" > ", (vec2){px, py}, C_PROMPT, C_BG);
    px += 3 * CHAR_W;

    wm_draw_text(input_buf, (vec2){px, py}, C_INPUT, C_BG);
    px += input_len * CHAR_W;

    wm_draw_fill_rec((rec){px, py, 2, CHAR_H}, C_CURSOR);
    wm_end_draw();
}