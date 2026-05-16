#include "vga.h"
#include "keyboard.h"
#include "kstring.h"

#define INPUT_MAX 256
#define N_COLORS 7

static int color_idx = 0;
static const vga_color_t fg_colors[] = {
    VGA_LIGHT_GREY, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED,  VGA_LIGHT_MAGENTA, VGA_LIGHT_BROWN, VGA_WHITE
};

static void draw_logo(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_println("  _____ _                          _  ____   _____ ");
    vga_println(" / ____(_)                        | |/ __ \\ / ____|");
    vga_println("| |  __ _ _ __ ___  _ __ ___   ___| | |  | | (___  ");
    vga_println("| | |_ | | '_ ` _ \\| '_ ` _ \\ / _ \\ | |  | |\\___ \\ ");
    vga_println("| |__| | | | | | | | | | | | |  __/ | |__| |____) |");
    vga_println(" \\_____|_|_| |_| |_|_| |_| |_|\\___|_|\\____/|_____/ ");
    vga_putchar('\n');
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void cmd_help(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_println("Available commands: ");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    vga_println("  help   — show this message");
    vga_println("  clear  — clear the screen");
    vga_println("  echo   — print text  (e.g. echo hello world)");
    vga_println("  color  — cycle terminal colors");
    vga_println("  reboot — soft reboot via keyboard controller");
    vga_println("  halt   — halt the CPU");
    vga_println("  info   — system info");
}

static void cmd_echo(const char *args) {
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_println(args);
}


static void cmd_info(void) {
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_println("GimmelOS v0.1  |  x86 32-bit protected mode");
    vga_println("Built with GCC + NASM + GRUB2");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void cmd_color(void) {
    color_idx = (color_idx + 1) % N_COLORS;
    vga_set_color(fg_colors[color_idx], VGA_BLACK);
    vga_println("Color changed! Type 'color' again to cycle.");
}

static void reboot(void) {
    uint8_t good = 0x02;
    while (good & 0x02) {
        __asm__ volatile("inb $0x64, %0" : "=a"(good));
    }

    __asm__ volatile("outb %0, $0x64" :: "a"((uint8_t)0xFE));
    __asm__ volatile("hlt");
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

void shell_run(void) {
    char input[INPUT_MAX];

    draw_logo();
    vga_println("Type 'help' for available commands.");

    while (1) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("0:\\");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_print("> ");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

        read_ln(input, INPUT_MAX);
        if (input[0] == '\0') continue;

        size_t cmd_len = 0;
        while (input[cmd_len] && input[cmd_len] != ' ') cmd_len++;
        
        const char* args = (input[cmd_len] == ' ') ? &input[cmd_len + 1] : "";

        if (kstrncmp(input, "help",   4) == 0) { cmd_help(); }
        else if (kstrncmp(input, "clear",  5) == 0) { vga_clear(); }
        else if (kstrncmp(input, "echo",   4) == 0) { cmd_echo(args); }
        else if (kstrncmp(input, "color",  5) == 0) { cmd_color(); }
        else if (kstrncmp(input, "info",   4) == 0) { cmd_info(); }
        else if (kstrncmp(input, "reboot", 6) == 0) { reboot(); }
        
        else if (kstrncmp(input, "halt",   4) == 0) {
            vga_println("Halting CPU. Goodbye.");
            __asm__ volatile ("cli; hlt");
        } 
        
        else {
            vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
            vga_print("Unknown command: ");
            vga_println(input);
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        }

    }
}

