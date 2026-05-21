#include "drivers/vga/vga.h"
#include "drivers/keyboard/keyboard.h"
#include "lib/kstring.h"
#include "lib/path.h"
#include "filesystem/fs.h"

#include "apps/editor.h"

#define CMD_IS(s) (cmd_len == sizeof(s)-1 && kstrncmp(input, s, cmd_len) == 0)

#define INPUT_MAX 256

static void cpuid(uint32_t code, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(code));
}

static void draw_logo(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_println("  _____ _                          _  ____   _____ ");
    vga_println(" / ____(_)                        | |/ __ \\ / ____|");
    vga_println("| |  __ _ _ __ ___  _ __ ___   ___| | |  | | (___  ");
    vga_println("| | |_ | | '_ ` _ \\| '_ ` _ \\ / _ \\ | |  | |\\___ \\ ");
    vga_println("| |__| | | | | | | | | | | | |  __/ | |__| |____) |");
    vga_println(" \\_____|_|_| |_| |_|_| |_| |_|\\___|_|\\____/|_____/ v0.1");
    vga_putchar('\n');
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void cmd_help(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_println("Available commands: ");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    vga_println("  help   - show this message");
    vga_println("  clear  - clear the screen");
    vga_println("  echo   - print text  (e.g. echo hello world)");
    vga_println("  reboot - soft reboot via keyboard controller");
    vga_println("  halt   - halt the CPU");
    vga_println("  info   - system info");
    vga_println("  ls     - files list");

    vga_println("");

    vga_println("  lito  [path]  - open lito editor to edit file");
    vga_println("  mk    [path]  - create file");
    vga_println("  mkdir [path]  - create directory");
    vga_println("  cat    [path] - read file");
    vga_println("  wr    [path]  - write line in a file");
}

static void read_ln(char *buf, size_t maxlen) {
    size_t i = 0;
    while (1) {
        char c = keyboard_getchar();
        if (c == '\n' || c == '\r') {
            buf[i] = '\0';
            vga_putchar('\n');
            return;
        } 
        
        else if (c == '\b') {
            if (i > 0) { i--; vga_putchar('\b'); }
        } 
        
        else if (i < maxlen - 1) {
            buf[i++] = c;
            vga_putchar(c);
        }
    }
}

static void cmd_cd(const char *args) {
    if (!args || args[0] == '\0' || (args[0] == '~' && args[1] == '\0')) {
        cwd_inode = 0;
        kstrncpy(cwd_path, "/", PATH_MAX);
        return;
    }

    char resolved_args[PATH_MAX];
    if (args[0] == '~' && args[1] == '/') {
        resolved_args[0] = '/';
        kstrncpy(resolved_args + 1, args + 2, PATH_MAX - 1);
        args = resolved_args;
    }

    int target = resolve_path(args);

    if (target < 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("cd: not found: ");
        vga_println(args);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return;
    }

    if (!fs_is_dir(target)) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("cd: not a directory: ");
        vga_println(args);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return;
    }

    cwd_inode = (uint32_t)target;
    fs_get_path(target, cwd_path, PATH_MAX);
}

static void cmd_ls(const char *args) {
    int target = (args && args[0]) ? resolve_path(args) : (int)cwd_inode;
    if (target < 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("ls: not found: ");
        vga_println(args);
        return;
    }

    fs_list((uint32_t)target);
}

static void cmd_mk(const char *args) {
    if (!args || !args[0]) { vga_println("mk: missing name"); return; }

    char dir[PATH_MAX]; char name[FS_MAX_NAME];
    fs_split_path(args, dir, name);
    int parent = (dir[0]) ? resolve_path(dir) : (int)cwd_inode;

    if (parent < 0) { vga_error("mk: parent dir not found"); return; }
    if (fs_mk(name, (uint32_t)parent) < 0) vga_error("mk: failed (disk full?)");
}

static void cmd_mkdir(const char *args) {
    if (!args || !args[0]) { vga_println("mkdir: missing name"); return; }

    char dir[PATH_MAX]; char name[FS_MAX_NAME];
    fs_split_path(args, dir, name);

    int parent = (dir[0]) ? resolve_path(dir) : (int)cwd_inode;
    if (parent < 0) { vga_error("mkdir: parent dir not found"); return; }

    if (fs_mkdir(name, (uint32_t)parent) < 0) vga_error("mkdir: failed (disk full?)");
}

static void cmd_cat(const char *args) {
    if (!args || !args[0]) { vga_println("cat: missing path"); return; }
    int id = resolve_path(args);
    if (id < 0) { vga_error("cat: file not found"); return; }

    uint8_t buf[512];
    kmemset(buf, 0, sizeof(buf));
    fs_read_by_id(id, buf);
    buf[511] = '\0';

    vga_println((char *)buf);
}

static void cmd_wr(const char *args, char *read_buf, size_t bufsz) {
    if (!args || !args[0]) { vga_println("wr: missing path"); return; }
    int id = resolve_path(args);
    if (id < 0) { vga_error("wr: file not found"); return; }

    vga_print("content: ");
    read_ln(read_buf, bufsz);
    fs_write_by_id(id, (uint8_t *)read_buf, kstrlen(read_buf));
}

static void cmd_echo(const char *args) {
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_println(args);
}


static void cmd_info(void) {
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);

    vga_println("=== GimmelOS SYSTEM INFO ===");

    vga_println("OS: GimmelOS v0.1");
    vga_println("Arch: x86 32-bit protected mode");
    vga_println("Bootloader: GRUB Multiboot");
    vga_println("Graphics: VGA text mode");
    vga_println("Input: PS/2 keyboard (polling)");
    vga_println("Memory model: flat (no paging)");

    uint32_t a,b,c,d;
    cpuid(0, &a,&b,&c,&d);

    vga_print("CPU vendor raw: ");
    vga_print_hex(a);
    vga_print(" ");
    vga_print_hex(b);
    vga_print(" ");
    vga_print_hex(d);
    vga_putchar('\n');

    vga_println("RAM: (requires multiboot memory map)");
    vga_println("Disk: ATA (LBA28)");
    vga_println("============================");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void reboot(void) {
    uint8_t good = 0x02;
    while (good & 0x02) {
        __asm__ volatile("inb $0x64, %0" : "=a"(good));
    }

    __asm__ volatile("outb %0, $0x64" :: "a"((uint8_t)0xFE));
    __asm__ volatile("hlt");
}

void shell_run(void) {
    char input[INPUT_MAX];
    char wbuf[512];

    draw_logo();
    vga_println("Type 'help' for available commands.");
    vga_putchar('\n');

    while (1) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print(cwd_path);
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_print(" > ");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

        read_ln(input, INPUT_MAX);
        if (input[0] == '\0') continue;

        size_t cmd_len = 0;
        while (input[cmd_len] && input[cmd_len] != ' ') cmd_len++;
        const char *args = (input[cmd_len] == ' ') ? &input[cmd_len + 1] : "";

        if      (CMD_IS("help"))   cmd_help();
        else if (CMD_IS("clear"))  vga_clear();
        else if (CMD_IS("echo"))   { vga_set_color(VGA_LIGHT_GREY, VGA_BLACK); vga_println(args); }
        else if (CMD_IS("info"))   cmd_info();
        else if (CMD_IS("pwd"))    vga_println(cwd_path);
        else if (CMD_IS("reboot")) reboot();
        else if (CMD_IS("halt"))   { vga_println("Halting. Goodbye."); __asm__ volatile("cli; hlt"); }
        else if (CMD_IS("ls"))     cmd_ls(args);
        else if (CMD_IS("cd"))     cmd_cd(args);
        else if (CMD_IS("mkdir"))  cmd_mkdir(args);
        else if (CMD_IS("mk"))     cmd_mk(args);
        else if (CMD_IS("cat"))    cmd_cat(args);
        else if (CMD_IS("wr"))     cmd_wr(args, wbuf, sizeof(wbuf));
        else {
            vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
            vga_print("Unknown command: ");
            vga_println(input);
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        }
    }
}